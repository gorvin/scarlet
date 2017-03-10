#include "scarlet/http/HTTPDefs.h"
#include <boost/algorithm/string/case_conv.hpp> //to_lower_copy
#include <boost/lexical_cast.hpp>
#include <boost/token_iterator.hpp>
#include <iostream>
#include <stdio.h> // sprintf

namespace scarlet {
namespace http {

// generic strings used by HTTP
std::string const HTTPDefs::STRING_EMPTY;
std::string const HTTPDefs::STRING_CRLF("\x0D\x0A");
std::string const HTTPDefs::STRING_HTTP_AND_SLASH("HTTP/");
std::string const HTTPDefs::STRING_COLON_AND_SPACE(": ");

// common HTTP header names
std::string const HTTPDefs::HEADER_HOST("Host");
std::string const HTTPDefs::HEADER_CONNECTION("Connection");
std::string const HTTPDefs::HEADER_CONTENT_TYPE("Content-Type");
std::string const HTTPDefs::HEADER_CONTENT_LENGTH("Content-Length");
std::string const HTTPDefs::HEADER_CONTENT_ENCODING("Content-Encoding");
std::string const HTTPDefs::HEADER_TRANSFER_ENCODING("Transfer-Encoding");
std::string const HTTPDefs::HEADER_AUTHORIZATION("Authorization");
std::string const HTTPDefs::HEADER_WWW_AUTHENTICATE("WWW-Authenticate");
std::string const HTTPDefs::HEADER_ETAG("Etag");
std::string const HTTPDefs::HEADER_IF_MATCH("If-Match");
std::string const HTTPDefs::HEADER_IF_NONE_MATCH("If-None-Match");

// common HTTP request methods
std::string const HTTPDefs::REQUEST_METHOD_HEAD("HEAD");
std::string const HTTPDefs::REQUEST_METHOD_GET("GET");
std::string const HTTPDefs::REQUEST_METHOD_PUT("PUT");
std::string const HTTPDefs::REQUEST_METHOD_POST("POST");
std::string const HTTPDefs::REQUEST_METHOD_DELETE("DELETE");

// common HTTP response messages
std::string const HTTPDefs::RESPONSE_MESSAGE_OK("OK");
std::string const HTTPDefs::RESPONSE_MESSAGE_CREATED("Created");
std::string const HTTPDefs::RESPONSE_MESSAGE_NO_CONTENT("No Content");
std::string const HTTPDefs::RESPONSE_MESSAGE_FOUND("Found");
std::string const HTTPDefs::RESPONSE_MESSAGE_UNAUTHORIZED("Unauthorized");
std::string const HTTPDefs::RESPONSE_MESSAGE_FORBIDDEN("Forbidden");
std::string const HTTPDefs::RESPONSE_MESSAGE_NOT_FOUND("Not Found");
std::string const HTTPDefs::RESPONSE_MESSAGE_METHOD_NOT_ALLOWED("Method Not Allowed");
std::string const HTTPDefs::RESPONSE_MESSAGE_NOT_MODIFIED("Not Modified");
std::string const HTTPDefs::RESPONSE_MESSAGE_BAD_REQUEST("Bad Request");
std::string const HTTPDefs::RESPONSE_MESSAGE_SERVER_ERROR("Server Error");
std::string const HTTPDefs::RESPONSE_MESSAGE_NOT_IMPLEMENTED("Not Implemented");
std::string const HTTPDefs::RESPONSE_MESSAGE_CONTINUE("Continue");

// common HTTP response codes
const unsigned int	HTTPDefs::RESPONSE_CODE_OK = 200;
const unsigned int	HTTPDefs::RESPONSE_CODE_CREATED = 201;
const unsigned int	HTTPDefs::RESPONSE_CODE_NO_CONTENT = 204;
const unsigned int	HTTPDefs::RESPONSE_CODE_FOUND = 302;
const unsigned int	HTTPDefs::RESPONSE_CODE_UNAUTHORIZED = 401;
const unsigned int	HTTPDefs::RESPONSE_CODE_FORBIDDEN = 403;
const unsigned int	HTTPDefs::RESPONSE_CODE_NOT_FOUND = 404;
const unsigned int	HTTPDefs::RESPONSE_CODE_METHOD_NOT_ALLOWED = 405;
const unsigned int	HTTPDefs::RESPONSE_CODE_NOT_MODIFIED = 304;
const unsigned int	HTTPDefs::RESPONSE_CODE_BAD_REQUEST = 400;
const unsigned int	HTTPDefs::RESPONSE_CODE_SERVER_ERROR = 500;
const unsigned int	HTTPDefs::RESPONSE_CODE_NOT_IMPLEMENTED = 501;
const unsigned int	HTTPDefs::RESPONSE_CODE_CONTINUE = 100;

// static member functions
bool HTTPDefs::base64_decode(const std::string &input, std::string &output)
{
	static const char nop = -1;
	static const char decoding_data[] = {
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop, 62, nop,nop,nop, 63,
		52, 53, 54,  55,  56, 57, 58, 59,  60, 61,nop,nop, nop,nop,nop,nop,
		nop, 0,  1,   2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
		15, 16, 17,  18,  19, 20, 21, 22,  23, 24, 25,nop, nop,nop,nop,nop,
		nop,26, 27,  28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
		41, 42, 43,  44,  45, 46, 47, 48,  49, 50, 51,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop,
		nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop, nop,nop,nop,nop
		};
	unsigned int input_length=input.size();
	const char * input_ptr = input.data();
	// allocate space for output string
	output.clear();
	output.reserve(((input_length+2)/3)*4);
	// for each 4-bytes sequence from the input, extract 4 6-bits sequences by droping first two bits
	// and regenerate into 3 8-bits sequence
	for (unsigned int i=0; i<input_length;i++) {
		char base64code0;
		char base64code1;
		char base64code2 = 0;   // initialized to 0 to suppress warnings
		char base64code3;
		base64code0 = decoding_data[static_cast<int>(input_ptr[i])];
		if(base64code0==nop)			// non base64 character
			return false;
		if(!(++i<input_length)) // we need at least two input bytes for first byte output
			return false;
		base64code1 = decoding_data[static_cast<int>(input_ptr[i])];
		if(base64code1==nop)			// non base64 character
			return false;
		output += ((base64code0 << 2) | ((base64code1 >> 4) & 0x3));
		if(++i<input_length) {
			char c = input_ptr[i];
			if(c =='=') { // padding , end of input
				BOOST_ASSERT( (base64code1 & 0x0f)==0);
				return true;
			}
			base64code2 = decoding_data[static_cast<int>(input_ptr[i])];
			if(base64code2==nop)			// non base64 character
				return false;
			output += ((base64code1 << 4) & 0xf0) | ((base64code2 >> 2) & 0x0f);
		}
		if(++i<input_length) {
			char c = input_ptr[i];
			if(c =='=') { // padding , end of input
				BOOST_ASSERT( (base64code2 & 0x03)==0);
				return true;
			}
			base64code3 = decoding_data[static_cast<int>(input_ptr[i])];
			if(base64code3==nop)			// non base64 character
				return false;
			output += (((base64code2 << 6) & 0xc0) | base64code3 );
		}
	}
	return true;
}

bool HTTPDefs::base64_encode(const std::string &input, std::string &output)
{
	static const char encoding_data[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned int input_length=input.size();
	const char * input_ptr = input.data();
	// allocate space for output string
	output.clear();
	output.reserve(((input_length+2)/3)*4);
	// for each 3-bytes sequence from the input, extract 4 6-bits sequences and encode using
	// encoding_data lookup table.
	// if input do not contains enough chars to complete 3-byte sequence,use pad char '='
	for (unsigned int i=0; i<input_length;i++) {
		int base64code0=0;
		int base64code1=0;
		int base64code2=0;
		int base64code3=0;
		base64code0 = (input_ptr[i] >> 2)  & 0x3f;	// 1-byte 6 bits
		output += encoding_data[base64code0];
		base64code1 = (input_ptr[i] << 4 ) & 0x3f;	// 1-byte 2 bits +
		if (++i < input_length) {
			base64code1 |= (input_ptr[i] >> 4) & 0x0f; // 2-byte 4 bits
			output += encoding_data[base64code1];
			base64code2 = (input_ptr[i] << 2) & 0x3f;  // 2-byte 4 bits +

			if (++i < input_length) {
				base64code2 |= (input_ptr[i] >> 6) & 0x03; // 3-byte 2 bits
				base64code3  = input_ptr[i] & 0x3f;		  // 3-byte 6 bits
				output += encoding_data[base64code2];
				output += encoding_data[base64code3];
			} else {
				output += encoding_data[base64code2];
				output += '=';
			}
		} else {
			output += encoding_data[base64code1];
			output += '=';
			output += '=';
		}
	}
	return true;
}

std::string HTTPDefs::url_decode(const std::string& str)
{
	char decode_buf[3];
	std::string result;
	result.reserve(str.size());
	for (std::string::size_type pos = 0; pos < str.size(); ++pos) {
		switch(str[pos]) {
		case '+':
			// convert to space character
			result += ' ';
			break;
		case '%':
			// decode hexidecimal value
			if (pos + 2 < str.size()) {
				decode_buf[0] = str[++pos];
				decode_buf[1] = str[++pos];
				decode_buf[2] = '\0';
				result += static_cast<char>( strtol(decode_buf, 0, 16) );
			} else {
				// recover from error by not decoding character
				result += '%';
			}
			break;
		default:
			// character does not need to be escaped
			result += str[pos];
		}
	};
	return result;
}

inline char HexDigitToChar(unsigned char hx)
{
	assert((hx & 0x0F) == hx);
	if (hx <= 9) return '0' + hx;
	return 'A' + hx - 10;
}

std::string HTTPDefs::url_encode(const std::string& str)
{
	char encode_buf[4];
	std::string result;
	encode_buf[0] = '%';
	encode_buf[3] = '\0';
	result.reserve(str.size());
	// character selection for this algorithm is based on the following url:
	// http://www.blooberry.com/indexdot/html/topics/urlencoding.htm
	for (std::string::size_type pos = 0; pos < str.size(); ++pos) {
		switch(str[pos]) {
		default:
			if (str[pos] > 32 && str[pos] < 127) {
				// character does not need to be escaped
				result += str[pos];
				break;
			}
			// else pass through to next case
		case ' ':
		case '$': case '&': case '+': case ',': case '/': case ':':
		case ';': case '=': case '?': case '@': case '"': case '<':
		case '>': case '#': case '%': case '{': case '}': case '|':
		case '\\': case '^': case '~': case '[': case ']': case '`':
			// the character needs to be encoded
			encode_buf[1] = HexDigitToChar((unsigned char)(str[pos] >> 4));
			encode_buf[2] = HexDigitToChar((unsigned char)(str[pos] & 0x0F));
			result += encode_buf;
			break;
		}
	};
	return result;
}

std::string HTTPDefs::version_string(unsigned short vmajor, unsigned short vminor)
{
    std::string http_version(HTTPDefs::STRING_HTTP_AND_SLASH);
    try {
        http_version += boost::lexical_cast<std::string>(vmajor);
        http_version += '.';
        http_version += boost::lexical_cast<std::string>(vminor);
    } catch (std::bad_cast& /*ex*/) {
        std::wclog << "Failed cast to string HTTP version values: " << vmajor << " and " << vminor << std::endl;
    }
    return http_version;
}

void HTTPDefs::version_values(unsigned short& major, unsigned short& minor, std::string const& vstr)
{
    size_t const slash(vstr.find('/'));
    size_t const dot(vstr.find('.'));
    major = 0;
    minor = 0;
    try {
        if(slash != std::string::npos && dot != std::string::npos && dot > slash) {
            major = boost::lexical_cast<unsigned short>(vstr.substr(slash+1, dot-slash-1));
            minor = boost::lexical_cast<unsigned short>(vstr.substr(dot+1));
        }
    } catch (std::bad_cast& /*ex*/) {
        std::wclog << "Failed cast from first line HTTP version values: '" << vstr.c_str() << "'" << std::endl;
    }
}

//std::string HTTPDefs::get_date_string(const time_t t)
//{
//	// use mutex since time functions are normally not thread-safe
//	static boost::mutex	time_mutex;
//	static const char *TIME_FORMAT = "%a, %d %b %Y %H:%M:%S GMT";
//	static const unsigned int TIME_BUF_SIZE = 100;
//	char time_buf[TIME_BUF_SIZE+1];
//
//	boost::mutex::scoped_lock time_lock(time_mutex);
//	if (strftime(time_buf, TIME_BUF_SIZE, TIME_FORMAT, gmtime(&t)) == 0)
//		time_buf[0] = '\0';	// failed; resulting buffer is indeterminate
//	time_lock.unlock();
//
//	return std::string(time_buf);
//}

std::size_t HeadersMultimap::lowercase_hash::operator()(std::string const& s) const
{
    return boost::hash<std::string>()(boost::algorithm::to_lower_copy(s));
}

bool HeadersMultimap::lowercase_equal_to::operator()(const std::string& __x, const std::string& __y) const
{
    //DBGMSGAT("Comparing '" << __x << "' against '" << __y << "'");
    return boost::to_lower_copy(__x) == boost::to_lower_copy(__y);
}

std::string const& HeadersMultimap::get(const std::string& name) const
{
    nocasemap_t::const_iterator i(m_map.find(name));
    return ( (i==m_map.end()) ? HTTPDefs::STRING_EMPTY : i->second );
}

//concatenate values of same named headers
void HeadersMultimap::normalize(void)
{
    for(nocasemap_t::iterator i(m_map.begin()); i != m_map.end(); ++i) {
        std::pair<nocasemap_t::iterator, nocasemap_t::iterator> res(m_map.equal_range(i->first));
        nocasemap_t::iterator j(res.first);
        if(j == m_map.end() || ++j == res.second) continue;
        for(; j != res.second; ++j)
            res.first->second.append(",").append(j->second);
        m_map.erase(++(j = res.first), res.second);// remove any remaining values
        i = m_map.begin(); //restart
    }
}

/** Changes the value for a header. Adds the header if it does not already exist. If
 * multiple values exist for the header, they will be removed and only the new value will remain.
 * @param name the header to change the value for
 * @param value the value to assign to the header
 */
void HeadersMultimap::set(std::string const& name, std::string const& value)
{
    // retrieve all current values for key
    std::pair<nocasemap_t::iterator, nocasemap_t::iterator> res(m_map.equal_range(name));
    if(res.first == m_map.end()) {// no values exist -> add a new key
        m_map.insert(std::make_pair(name, value));
    } else { // set the first value found for the key to the new one
        res.first->second = value;
        ++(res.first);
        m_map.erase(res.first, res.second);// remove any remaining values
    }
}

void HeadersMultimap::serialize(std::vector<boost::asio::const_buffer>& buffer) const
{
    // append HTTP headers
    for (nocasemap_t::const_iterator i(m_map.begin()); i != m_map.end(); ++i) {
        buffer.push_back(boost::asio::buffer(i->first));
        buffer.push_back(boost::asio::buffer(HTTPDefs::STRING_COLON_AND_SPACE));
        buffer.push_back(boost::asio::buffer(i->second));
        buffer.push_back(boost::asio::buffer(HTTPDefs::STRING_CRLF));
    }
}

void HeadersMultimap::dump(void) const
{
    for (nocasemap_t::const_iterator i(m_map.begin()); i != m_map.end(); ++i) {
        std::wclog << i->first.c_str()
			<< HTTPDefs::STRING_COLON_AND_SPACE.c_str()
			<< i->second.c_str()
			<< HTTPDefs::STRING_CRLF.c_str();
    }
}

RequestLine::RequestLine(std::string const& line)
: method()
, resource_uri()
, resource_query()
, version_major(0)
, version_minor(0)
{
    typedef boost::token_iterator<
        boost::char_separator<char>, std::string::const_iterator, std::string
    > string_token_iterator;

    boost::char_separator<char> const sep(" \r");//SP or CR
    string_token_iterator it(sep, line.begin(), line.end());
    string_token_iterator const itend(sep, line.end(), line.end());
    if(it == itend) return;
    method = *it;
    if(++it == itend) return;
    size_t const pos(it->find('?'));
    resource_uri = it->substr(0, pos);
    if(pos != std::string::npos)
        resource_query = it->substr(pos+1);

    if(++it == itend) return;
    HTTPDefs::version_values(version_major, version_minor, *it);
}

RequestLine::RequestLine(
    std::string const& method
    , std::string const& resource_uri
    , std::string const& resource_query
    , unsigned short version_major
    , unsigned short version_minor
)
: method(method)
, resource_uri(resource_uri)
, resource_query(resource_query)
, version_major(version_major)
, version_minor(version_minor)
{ }

std::string RequestLine::string(void) const
{
    std::string line(method);
    line += ' ';
    line += resource_uri;
    if(!resource_query.empty()) {
        line += '?';
        line += resource_query;
    }
    line += ' ';
    line += HTTPDefs::version_string(version_major, version_minor);
    return line;
}

StatusLine::StatusLine(std::string const& line)
: version_major(0)
, version_minor(0)
, code(0)
, message()
{
    typedef boost::token_iterator<
        boost::char_separator<char>, std::string::const_iterator, std::string
    > string_token_iterator;
    boost::char_separator<char> const sep(" \r");
    string_token_iterator it(sep, line.begin(), line.end());
    string_token_iterator const itend(sep, line.end(), line.end());
    if(it == itend) return;
    HTTPDefs::version_values(version_major, version_minor, *it);
    if(++it == itend) return;
    try {
        code = boost::lexical_cast<unsigned int>(*it);
    } catch (std::bad_cast& /*ex*/) {
        std::wclog << "Failed cast from status line code value: '" << it->c_str() << "'" << std::endl;
    }
    if(++it == itend) return;
    message = *it;
}

StatusLine::StatusLine(
    unsigned short version_major
    , unsigned short version_minor
    , unsigned int code
    , std::string message
)
: version_major(version_major)
, version_minor(version_minor)
, code(code)
, message(message)
{ }

std::string StatusLine::string(void) const
{
    std::string line(HTTPDefs::version_string(version_major, version_minor));
    line += ' ';
    try {
        line += boost::lexical_cast<std::string>(code);
    } catch (std::bad_cast& /*ex*/) {
        std::wclog << "Failed cast response code to string: '" << code << "'" << std::endl;
    }
    line += ' ';
    line += message;
    return line;
}

}
}
