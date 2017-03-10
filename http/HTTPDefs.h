#ifndef HTTP_DEFS_H
#define HTTP_DEFS_H
#include <boost/unordered/unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio/buffer.hpp>
#include <string>
#include <map>
#include <bmu/tydefs.h>

namespace scarlet {
namespace http {
using ::bmu::u8unit_t;
using ::bmu::u8vector_t;
using ::bmu::u8vector_it;
using ::bmu::rawcontent_t;

enum response_code_e {
	HTTP_OK = 200,
	HTTP_OK_CREATED = 201,
	HTTP_FAIL_BAD_REQUEST = 400, ///< invalid HTTP request
	HTTP_FAIL_AUTHORIZATION = 401,///< unauthorized
	HTTP_FAIL_NOT_FOUND = 404,///< Not Found
	HTTP_FAIL_NOT_ALLOWED = 405,///< Method Not Allowed
	HTTP_FAIL_CONSTRAINTS = 409, /// <Conflict
	HTTP_FAIL_IF_PERFORM = 412,///< Precondition Failed
	HTTP_FAIL_MIME = 415,///< Unsupported Media Type
	HTTP_ERROR_INTERNAL = 500 ///< Internal Server Error
};

struct httpresponse_t {
	response_code_e code;
	u8vector_t      body;//trazeni dokument/fragment ili sadrzaj za 409 odziv
	std::string     etag;//etag za body
	std::string     mime;//samo za GET je bitno
	std::map<std::string, std::string> extra_hdrs;
};

struct httprequest_t {
	enum method_e {
		RQ_METHOD_GET,
		RQ_METHOD_PUT,
		RQ_METHOD_DELETE
		//TODO: RQ_METHOD_POST //OMA XDM V2.1, XDCP (XDM Command Protocol) document
		//TODO: RQ_SUBSCRIBE //OMA XDM V2.1, SIP/IP Core
	};
	method_e                 method;
	std::string              username;
	std::string              domain;
	rawcontent_t             body;
	std::string              mime;
	std::vector<std::string> ifetags;
	std::vector<std::string> ifnoetags;
};

class MsgParser;
class MsgSerializer;
class MsgReader;
class MsgWriter;

typedef boost::shared_ptr<MsgParser>        MsgParserPtr;
typedef boost::shared_ptr<MsgSerializer>    MsgSerializerPtr;
typedef boost::shared_ptr<MsgReader>        MsgReaderPtr;
typedef boost::shared_ptr<MsgWriter>        MsgWriterPtr;

/// HTTPDefs: common data types used by HTTP
struct HTTPDefs {
    static size_t const DEFAULT_MAX_BODY_SIZE = 1024 * 1024;	// 1 MB

	// generic strings used by HTTP
	static std::string const STRING_EMPTY;
	static std::string const STRING_CRLF;
	static std::string const STRING_HTTP_AND_SLASH;
	static std::string const STRING_COLON_AND_SPACE;

	// common HTTP header names
	static std::string const HEADER_HOST;
	static std::string const HEADER_CONNECTION;
	static std::string const HEADER_CONTENT_TYPE;
	static std::string const HEADER_CONTENT_LENGTH;
	static std::string const HEADER_CONTENT_ENCODING;
	static std::string const HEADER_TRANSFER_ENCODING;
	static std::string const HEADER_AUTHORIZATION;
	static std::string const HEADER_WWW_AUTHENTICATE;
	static std::string const HEADER_ETAG;
	static std::string const HEADER_IF_MATCH;
	static std::string const HEADER_IF_NONE_MATCH;

	// common HTTP request methods
	static std::string const REQUEST_METHOD_HEAD;
	static std::string const REQUEST_METHOD_GET;
	static std::string const REQUEST_METHOD_PUT;
	static std::string const REQUEST_METHOD_POST;
	static std::string const REQUEST_METHOD_DELETE;

	// common HTTP response messages
	static std::string const RESPONSE_MESSAGE_OK;
	static std::string const RESPONSE_MESSAGE_CREATED;
	static std::string const RESPONSE_MESSAGE_NO_CONTENT;
	static std::string const RESPONSE_MESSAGE_FOUND;
	static std::string const RESPONSE_MESSAGE_UNAUTHORIZED;
	static std::string const RESPONSE_MESSAGE_FORBIDDEN;
	static std::string const RESPONSE_MESSAGE_NOT_FOUND;
	static std::string const RESPONSE_MESSAGE_METHOD_NOT_ALLOWED;
	static std::string const RESPONSE_MESSAGE_NOT_MODIFIED;
	static std::string const RESPONSE_MESSAGE_BAD_REQUEST;
	static std::string const RESPONSE_MESSAGE_SERVER_ERROR;
	static std::string const RESPONSE_MESSAGE_NOT_IMPLEMENTED;
	static std::string const RESPONSE_MESSAGE_CONTINUE;

	// common HTTP response codes
	static unsigned int const RESPONSE_CODE_OK;
	static unsigned int const RESPONSE_CODE_CREATED;
	static unsigned int const RESPONSE_CODE_NO_CONTENT;
	static unsigned int const RESPONSE_CODE_FOUND;
	static unsigned int const RESPONSE_CODE_UNAUTHORIZED;
	static unsigned int const RESPONSE_CODE_FORBIDDEN;
	static unsigned int const RESPONSE_CODE_NOT_FOUND;
	static unsigned int const RESPONSE_CODE_METHOD_NOT_ALLOWED;
	static unsigned int const RESPONSE_CODE_NOT_MODIFIED;
	static unsigned int const RESPONSE_CODE_BAD_REQUEST;
	static unsigned int const RESPONSE_CODE_SERVER_ERROR;
	static unsigned int const RESPONSE_CODE_NOT_IMPLEMENTED;
	static unsigned int const RESPONSE_CODE_CONTINUE;

	/** base64 decoding , used internally by ResourceAuth
	 * @param input - base64 encoded string
	 * @param output - decoded string ( may include non-text chars)
	 * @return true if successful, false if input string contains non-base64 symbols
	 */
	static bool base64_decode(std::string const &input, std::string & output);

	/** base64 encoding , used internally by ResourceAuth
	 * @param input - arbitrary string ( may include non-text chars)
	 * @param output - base64 encoded string
	 * @return true if successful,
	 */
	static bool base64_encode(std::string const &input, std::string & output);
	/// escapes URL-encoded strings (a%20value+with%20spaces)
	static std::string url_decode(const std::string& str);
	/// encodes strings so that they are safe for URLs (with%20spaces)
	static std::string url_encode(const std::string& str);
    /// returns a string representation of the HTTP version (i.e. "HTTP/1.1")
    static std::string version_string(unsigned short vmajor, unsigned short vminor);
    static void version_values(unsigned short& major, unsigned short& minor, std::string const& vstr);

//	/// converts time_t format into an HTTP-date string
//	static std::string get_date_string(const time_t t);
};

///data type for case-insensitive header-value pairs
class HeadersMultimap {
    struct lowercase_hash : public std::unary_function<std::string, std::size_t> {
        std::size_t operator()(std::string const& s) const;
    };
    struct lowercase_equal_to : public std::binary_function<std::string, std::string, bool>
    {
        bool operator()(const std::string& __x, const std::string& __y) const;
    };
    typedef boost::unordered_multimap<std::string, std::string, lowercase_hash, lowercase_equal_to> nocasemap_t;
    nocasemap_t m_map;
public:
	/** Returns the first value in a dictionary if key is found; or an empty
	 * string if no values are found
	 * @param name the key to search for
	 * @return value if found; empty string if not
	 */
	std::string const& get(const std::string& name) const;
    ///concatenate values of same named headers
    void normalize(void);

	/** Changes the value for a header. Adds the header if it does not already exist. If
     * multiple values exist for the header, they will be removed and only the new value will remain.
	 * @param name the header to change the value for
	 * @param value the value to assign to the header
	 */
    void set(std::string const& name, std::string const& value);
    void add(std::string const& name, std::string const& value)
    {
        m_map.insert(std::make_pair(name, value));
    }

	/** Deletes all values for a key
	 * @param key the key to delete
	 */
	void erase(const std::string& name) { m_map.erase(name); }
	void clear(void) { m_map.clear(); }
	bool empty(void) const { return m_map.empty(); }

    void serialize(std::vector<boost::asio::const_buffer>& buffer) const;

    void dump(void) const;
};

struct RequestLine {
    std::string method;
    std::string resource_uri;
    std::string resource_query;
    unsigned short version_major;
    unsigned short version_minor;

    std::string string(void) const;

    RequestLine(std::string const& line);

    RequestLine(
        std::string const& method
        , std::string const& uri
        , std::string const& query
        , unsigned short vmajor
        , unsigned short vminor
    );
};

struct StatusLine {
    unsigned short version_major;
    unsigned short version_minor;
    unsigned int code;
    std::string message;
    std::string string(void) const;
    StatusLine(std::string const& line);
    StatusLine(unsigned short vmajor, unsigned short vminor, unsigned int code, std::string message);
};

}
}

#endif // HTTP_DEFS_H
