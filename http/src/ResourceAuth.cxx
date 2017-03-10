#include "scarlet/http/ResourceAuth.h"
#include "scarlet/xcap/XCAccess.h"
#include "scarlet/xcap/Storage.h"
#include "scarlet/http/DigestAuthParams.h"
#include "scarlet/http/MsgParser.h"
#include "bmu/Logger.h"
#include <boost/algorithm/string.hpp>
#include <boost/token_iterator.hpp>

namespace scarlet {
namespace http {

ResourceAuth::auth_status_t ResourceAuth::handleBasicAuthentication(
	ResolveUserFn resolveUser
	, http::MsgParserPtr http_request
    , std::string const& domain
    , std::string const& authorization
    , boost::posix_time::ptime const& time_now
)
{
    std::string credentials;
    if ((credentials = authorization.substr(6)).empty()) {
        DBGMSGAT("Empty user credentials in HTTP request");
        return AUTH_FAIL;
    }

    {   boost::mutex::scoped_lock cache_lock(m_cache_mutex);
        UserCredentialsCache::iterator user_cache_ptr(m_user_cache.find(credentials+'@'+domain));
        if (user_cache_ptr!=m_user_cache.end()) {
            DBGMSGAT("Found user in cache of authenticated users");
            // we found the credentials in our cache..., we can approve authorization now!
            http_request->set_username(user_cache_ptr->second.second);
            user_cache_ptr->second.first = time_now;
            return AUTH_OK;
        }
    }

    std::string username;
    std::string password;

    {
        std::string user_password;

        if (!http::HTTPDefs::base64_decode(credentials, user_password)) {
            DBGMSGAT("Failed decoding Basic credentials");
            return AUTH_FAIL;
        }

        std::string::size_type const i(user_password.find(':'));
        if (i==0 || i==std::string::npos) {
            DBGMSGAT("Bad Basic credentials format, there is no ':' separator");
            return AUTH_FAIL;
        }
        username = user_password.substr(0, i);
        password = user_password.substr(i+1);
    }

	std::string const usernameAtDomain(username + '@' + domain);
    DBGMSGAT("Parsed credentials, searching for user " << usernameAtDomain << " in storage");

    // match username/password
    std::string stored_digest;
	//if(storage->user(stored_digest, username + '@' + domain) != 0)
	if(!resolveUser(boost::ref(stored_digest), boost::cref(usernameAtDomain))) {
        DBGMSGAT("Error in database");
        return AUTH_FAIL;
    }

    if(stored_digest.empty()) {
        DBGMSGAT("No user in database: " << usernameAtDomain);
        return AUTH_FAIL;
    }

	DBGMSGAT("stored password: " << stored_digest.c_str());

    if(stored_digest != password) {
        DBGMSGAT("Recived password not match for username: " << (username + ':' + password + '@' + domain));
        return AUTH_FAIL;
    }

    DBGMSGAT("User credentials supplied in HTTP request looks OK");

    // add user to the cache
    {
        boost::mutex::scoped_lock cache_lock(m_cache_mutex);
        m_user_cache.insert(std::make_pair(credentials + '@' + domain, std::make_pair(time_now, username)));
    }
    // add user credentials to the request object
    http_request->set_username(username);

    DBGMSGAT("Created User and added to HTTP request");

    return AUTH_OK;
}

void ResourceAuth::add_sent_nonce(std::string const& nonce)
{
    boost::mutex::scoped_lock cache_lock(m_cache_mutex);
    sent_nonce_time[nonce] = boost::posix_time::second_clock::universal_time();
}


ResourceAuth::auth_status_t ResourceAuth::handleDigestAuthentication(
	ResolveUserFn resolveUser
	, http::MsgParserPtr http_request
    , std::string const& domain
    , std::string const& authorization
    , boost::posix_time::ptime const& time_now
)
{
    std::string credentials;
    if ((credentials = authorization.substr(7)).empty()) {
        DBGMSGAT("Empty user credentials in HTTP request");
        return AUTH_FAIL;
    }

    // Parse authorization header

    typedef boost::token_iterator<
        boost::char_separator<char>, std::string::const_iterator, std::string
    > string_token_iterator;

    boost::char_separator<char> const sep_comma(",");
    string_token_iterator it(sep_comma, credentials.begin(), credentials.end());
    string_token_iterator const itend(sep_comma, credentials.end(), credentials.end());

    DigestAuthParams params;

    for(; it!=itend; ++it) {

        std::string::size_type const i(it->find('='));

        if (i==0 || i==std::string::npos) {
            DBGMSGAT("Bad Digest credentials format, there is no '=' separator");
            return AUTH_FAIL;
        }

        if(!params.add_parameter(it->substr(0, i), it->substr(i + 1)))
            return AUTH_FAIL;
    }

	if (!params.get_realm().empty() && params.get_realm() != domain) {
		DBGMSGAT("Secified user realm '" << params.get_realm().c_str() << "' differs from servers current domain '" << domain.c_str() << "'");
		return AUTH_BAD;
	}

	if (!params.get_algorithm().empty() && params.get_algorithm() != "MD5") {
		DBGMSGAT("Secified algorithm '" << params.get_algorithm().c_str() << "', currently only MD5 is supported");
		return AUTH_BAD;
	}

    {
        boost::mutex::scoped_lock cache_lock(m_cache_mutex);
        //CHECK: This is not same as for Basic Authentication caching but i think it is consistent
        UserCredentialsCache::iterator user_cache_ptr(m_user_cache.find(params.get_username()+'@'+domain));
        if(user_cache_ptr!=m_user_cache.end()) {
            DBGMSGAT("Found user in cache of authenticated users");
            // we found the credentials in our cache..., we can approve authorization now!
            http_request->set_username(user_cache_ptr->second.second);
            user_cache_ptr->second.first = time_now;
            return AUTH_OK;
        }
    }

/* The authenticating server must assure that the resource designated by the "uri" directive is the
same as the resource specified in the Request-Line; if they are not, the server SHOULD return a 400
Bad Request error. (Since this may be a symptom of an attack, server implementers may want to
consider logging such errors.) The purpose of duplicating information from the request URL in this
field is to deal with the possibility that an intermediate proxy may alter the client's Request-Line.
This altered (but presumably semantically equivalent) request would not result in the same digest as
that calculated by the client.
*/
    {   //does uri match
        std::string::size_type const pos(params.get_uri().find('?'));

        if(params.get_uri().substr(0, pos) != http_request->get_requested_resource()
           || (pos != std::string::npos
               && params.get_uri().substr(pos+1) != http_request->get_requested_query())) {
            DBGMSGAT("URI from Digest credentials does not match URI from request-line, bad request (400)");
            return AUTH_BAD;
        }
    }

    {   //is nonce expired
        std::map<std::string, boost::posix_time::ptime>::const_iterator itn(sent_nonce_time.find(params.get_nonce()));

        if(itn == sent_nonce_time.end()
           || time_now > itn->second + boost::posix_time::seconds(NONCE_EXPIRATION)) {
            DBGMSGAT("Bad Digest credentials, received nonce already expired or unknown");
            return AUTH_FAIL;
        }
    }

    // match username/password
	std::string const usernameAtDomain(params.get_username() + '@' + domain);
    std::string stored_pass;
    //if(storage->user(stored_pass, params.get_username()+'@'+domain) != 0) {
	if (!resolveUser(boost::ref(stored_pass), boost::cref(usernameAtDomain))) {
		DBGMSGAT("Error in database");
        return AUTH_FAIL;
    }

    if(stored_pass.empty()) {
        DBGMSGAT("No user in database: " << usernameAtDomain);
        return AUTH_FAIL;
    }

    if(!params.check(http_request->get_requested_method(), domain, stored_pass)) {
        DBGMSGAT("Recived credentials does not match credentials stored in database for user: "
                 << (params.get_username() + ':' + stored_pass + '@' + domain));
        return AUTH_FAIL;
    }

    DBGMSGAT("User credentials supplied in HTTP request looks OK");

    // add user to the cache
    {
        boost::mutex::scoped_lock cache_lock(m_cache_mutex);
        m_user_cache.insert(std::make_pair(params.get_username() + '@' + domain, std::make_pair(time_now, params.get_username())));
    }

    // add user credentials to the request object
    http_request->set_username(params.get_username());

    DBGMSGAT("Created User and added to HTTP request");

    return AUTH_OK;
}


ResourceAuth::auth_status_t ResourceAuth::handleAuthentication(ResolveUserFn resolveUser, http::MsgParserPtr http_request, std::string const& domain)
{
	boost::posix_time::ptime time_now(boost::posix_time::second_clock::universal_time());
	if (time_now > m_cache_cleanup_time + boost::posix_time::seconds(CACHE_EXPIRATION)) {
		// expire cache
		boost::mutex::scoped_lock cache_lock(m_cache_mutex);
		UserCredentialsCache::iterator i;
		UserCredentialsCache::iterator next=m_user_cache.begin();
		while (next!=m_user_cache.end()) {
			i=next;
			++next;
			if (time_now > i->second.first + boost::posix_time::seconds(CACHE_EXPIRATION))
				m_user_cache.erase(i);// expire old record now
		}

		std::map<std::string, boost::posix_time::ptime>::iterator ittmp;
		std::map<std::string, boost::posix_time::ptime>::iterator it(sent_nonce_time.begin());
		while (it!=sent_nonce_time.end()) {
			ittmp=it;
			++it;
			if(time_now > ittmp->second + boost::posix_time::seconds(NONCE_EXPIRATION))
                sent_nonce_time.erase(ittmp);
		}

		m_cache_cleanup_time = time_now;
	}

	// if we are here, we need to check if access authorized...
	std::string authorization(http_request->get_header(http::HTTPDefs::HEADER_AUTHORIZATION));
	if (authorization.empty()) {
        DBGMSGAT("There's no authorization header in HTTP request");
        return AUTH_FAIL;
	}

	if(boost::algorithm::starts_with(authorization, "Basic ")) {

        DBGMSGAT("Requested HTTP Basic Authentication");
        return handleBasicAuthentication(resolveUser, http_request, domain, authorization, time_now);

    } else if(boost::algorithm::starts_with(authorization, "Digest ")) {

        DBGMSGAT("Requested HTTP Digest Authentication");
        return handleDigestAuthentication(resolveUser, http_request, domain, authorization, time_now);

    }

    DBGMSGAT("Unknown Requested HTTP authentication metod");

    return AUTH_FAIL;
}

}
}
