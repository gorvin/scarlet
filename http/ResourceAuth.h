#include <scarlet/http/HTTPDefs.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/function.hpp>

namespace scarlet {
namespace http
{

typedef boost::function<bool(std::string&, std::string const&)> ResolveUserFn;

class ResourceAuth {
public:
    enum auth_status_t {
        AUTH_OK,
        AUTH_BAD,  //400
        AUTH_FAIL, //401
    };
private:
    enum { CACHE_EXPIRATION = 300 }; // 5 minutes
    enum { NONCE_EXPIRATION = 20 }; // 20 seconds
    /// time of the last cache clean up
	boost::posix_time::ptime				m_cache_cleanup_time;
	typedef std::map<std::string, std::pair<boost::posix_time::ptime, std::string> >  UserCredentialsCache;
    std::map<std::string, boost::posix_time::ptime> sent_nonce_time;
	/// cache of users that are currently active
	UserCredentialsCache				m_user_cache;
	/// mutex used to protect access to the user cache
	mutable boost::mutex		m_cache_mutex;
    auth_status_t handleBasicAuthentication(
		ResolveUserFn resolveUser
        , http::MsgParserPtr http_request
        , std::string const& domain
        , std::string const& authorization
        , boost::posix_time::ptime const& time_now
    );
    auth_status_t handleDigestAuthentication(
		ResolveUserFn resolveUser
		, http::MsgParserPtr http_request
        , std::string const& domain
        , std::string const& authorization
        , boost::posix_time::ptime const& time_now
    );
public:
    auth_status_t handleAuthentication(ResolveUserFn resolveUser, http::MsgParserPtr http_request, std::string const& domain);
    void add_sent_nonce(std::string const& nonce);
    ResourceAuth(void)
     : m_cache_cleanup_time(boost::posix_time::second_clock::universal_time())
     { }
};

}
}
