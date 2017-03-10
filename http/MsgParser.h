#ifndef MSG_PARSER_H
#define MSG_PARSER_H
#include "scarlet/http/HTTPDefs.h"

namespace scarlet {
namespace http {

class MsgHeadersParser {
    enum state_e {
        WANTED_START,
        WANTED_CR,
        WANTED_LF,
        WANTED_NOTHING,
    };
    state_e                         m_state;
    std::vector<std::string>        m_headers;
	HeadersMultimap                 m_headers_map;
    mutable boost::shared_ptr<RequestLine>  m_rqline;//for request
    mutable boost::shared_ptr<StatusLine>   m_stline;//for response
    static inline bool is_white(int c) { return(c == ' ' || c == '\t'); }
    static inline bool is_white_or_crlf(int c) { return(c == ' ' || c == '\t' || c == 0x0d || c == 0x0a); }
public:
    void reset(void)
    {
        m_state = WANTED_START;
        m_headers.clear();
        m_headers_map.clear();
        m_rqline.reset();
        m_stline.reset();
    }
    RequestLine const* get_request_line(void) const;
    StatusLine const* get_status_line(void) const;
	size_t parse(char const* data, size_t length);
    //std::string const& get_line(std::string const& field) const;
    std::string const& get_field(std::string const& name) const { return m_headers_map.get(name); }
    bool checkKeepAlive(bool is_request) const;
    bool is_chunked(void) const;
    int get_content_length(void) const;
	//HeadersMultimap const& get_headers(void) const { return m_headers; }
    bool is_finished(void) const { return m_state == WANTED_NOTHING; }
    void dump(void) const { m_headers_map.dump(); }
};


class MsgChunksParser {
    enum state_e {
        CHUNK_SIZE_WANTED_START,
        CHUNK_SIZE_WANTED_CR,
        CHUNK_SIZE_WANTED_LF,
        CHUNK_DATA_WANTED_START,
        CHUNK_DATA_WANTED_CR,
        CHUNK_DATA_WANTED_LF,
        CHUNK_FINISHED,
    };
    size_t const             m_max_body_size;
    state_e                  m_state;
    size_t                   m_remaining;
    std::string              m_tmp;
	std::vector<char>        m_body;
    //static inline bool is_white(char c) { return(c == ' ' || c == '\t'); }
    static inline bool is_hex(char c)
    {
        static std::string const hex("0123456789abcdefABCDEF");
        return hex.find(c) != std::string::npos ;
    }
    static inline bool is_hex_no_zero(char c) { return (c != '0' && is_hex(c)); }
public:
    MsgChunksParser(size_t max_body_size = HTTPDefs::DEFAULT_MAX_BODY_SIZE)
    : m_max_body_size(max_body_size)
    { }
    void reset(void)
    {
        m_state = CHUNK_SIZE_WANTED_START;
        m_remaining = 0;
        m_tmp.clear();
        m_body.clear();
    }
	size_t parse(char const* data, size_t const length);
	std::vector<char> const get_body(void) const { return m_body; }
    void swap_body(std::vector<char>& body2) { m_body.swap(body2); }
    ///relevantno samo ako se ne cita skroz do zatvaranja konekcije
    bool is_finished(void) const { return /*!m_remaining && */m_state == CHUNK_FINISHED; }
};


class MsgBodyParser {
    size_t const             m_max_body_size;
    int                      m_remaining;
	std::vector<char>        m_body;
public:
    MsgBodyParser(size_t max_body_size = HTTPDefs::DEFAULT_MAX_BODY_SIZE)
    : m_max_body_size(max_body_size)
    { }
    void reset(int remaining)
    {
        m_remaining = remaining;
        m_body.clear();
    }
	size_t parse(char const* data, size_t const length);
	std::vector<char> const get_body(void) const { return m_body; }
    void swap_body(std::vector<char>& body2) { m_body.swap(body2); }
    void set_finished(void);
    ///relevantno samo ako se ne cita skroz do zatvaranja konekcije
    bool is_finished(void) const { return !m_remaining; }
    int get_remaining(void) const { return m_remaining; }
};

class MsgParser {
    MsgHeadersParser     m_head_parser;
    MsgChunksParser m_chunked_parser;
    MsgBodyParser        m_body_parser;
    enum state_e {
        WANTED_HEADERS,
        WANTED_CHUNKS,
        WANTED_BODY,
        FAILED,
        FINISHED,
    };
    state_e              m_state;
    std::vector<char>    m_body;
    bool                 m_is_request;
    std::string          m_response_for_requested_method;
    std::string          m_authenticated_user;//username
    bool is_header_only(void) const;
public:
    MsgParser(size_t max_body_size = HTTPDefs::DEFAULT_MAX_BODY_SIZE)
    : m_head_parser()
    , m_chunked_parser(max_body_size)
    , m_body_parser(max_body_size)
    { }
    void reset(bool is_request, std::string const& response_for_requested_method = std::string())
    {
        m_head_parser.reset();
        m_chunked_parser.reset();
        m_body_parser.reset(0);
        m_state = WANTED_HEADERS;
        m_is_request = is_request;
        m_response_for_requested_method = response_for_requested_method;
        m_authenticated_user.clear();
    }
	size_t parse(char const* data, size_t const length);
    std::string get_header(std::string const& field) const { return m_head_parser.get_field(field); }
    bool checkKeepAlive(void) const { return m_head_parser.checkKeepAlive(m_is_request); }
	std::vector<char> const& get_body(void) const { return m_body; }
    std::string get_requested_method(void) const;
    std::string get_requested_resource(void) const;
    std::string get_requested_query(void) const;
    unsigned short get_major(void) const;
    unsigned short get_minor(void) const;
    unsigned int get_response_code(void) const;
    std::string get_response_message(void) const;
    void set_username(std::string const& user) { m_authenticated_user = user; }
    std::string const& get_username(void) const { return m_authenticated_user; }
    void set_finished(void);
    bool is_unacceptable_eof(void) const
    {
        return !(m_state == WANTED_BODY && m_body_parser.get_remaining() <= 0);
    }
    ///relevantno samo ako se ne cita skroz do zatvaranja konekcije
    bool is_finished(void) const { return m_state == FINISHED; }
    bool is_failed(void) const { return m_state == FAILED; }
    void dump_header(void) const { m_head_parser.dump(); }
};

}
}

#endif //MSG_PARSER_H
