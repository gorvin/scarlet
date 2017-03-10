#include "scarlet/http/MsgParser.h"
#include "bmu/Logger.h"
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/case_conv.hpp> //to_lower_copy
#include <boost/lexical_cast.hpp>
#include <iostream>

namespace scarlet {
namespace http {

bool MsgHeadersParser::is_chunked(void) const
{
    std::string const& line(get_field(HTTPDefs::HEADER_TRANSFER_ENCODING));
    return !line.empty() && boost::algorithm::to_lower_copy(line).find("chunked") != std::string::npos;
}

int MsgHeadersParser::get_content_length(void) const
{
    std::string const& value(get_field(HTTPDefs::HEADER_CONTENT_LENGTH));
    if(value.empty()) return -1;
    try {
        return boost::lexical_cast<int>(value);
    } catch (std::bad_cast& /*ex*/) {
        std::wclog << "Failed cast from Content-lenght header field value: '" << value << "'" << std::endl;
    }
    return -1;
}

RequestLine const* MsgHeadersParser::get_request_line(void) const
{
    if(m_headers.empty()) return 0;
    if(!m_rqline)
        m_rqline = boost::shared_ptr<RequestLine>(new RequestLine(m_headers[0]));
    return m_rqline.get();
}


StatusLine const* MsgHeadersParser::get_status_line(void) const
{
    if(m_headers.empty()) return 0;
    if(!m_stline)
        m_stline = boost::shared_ptr<StatusLine>(new StatusLine(m_headers[0]));
    return m_stline.get();
}

bool MsgHeadersParser::checkKeepAlive(bool is_request) const
{
    if(m_headers.empty()) return false;
    std::string conn(get_field(HTTPDefs::HEADER_CONNECTION));
    unsigned short vmajor(0);
    unsigned short vminor(0);
    if(is_request) {
        RequestLine const* r(get_request_line());
        if(r) {
            vmajor = r->version_major;
            vminor = r->version_minor;
        }
    } else {
        StatusLine const* s(get_status_line());
        if(s) {
            vmajor = s->version_major;
            vminor = s->version_minor;
        }
    }
	bool const bNewBehavior = (vmajor > 1 || (vmajor = 1 && vminor >= 1));
	if (bNewBehavior)
		return conn != "close";
    return conn == "Keep-Alive";
}

size_t MsgHeadersParser::parse(const char* data, size_t length)
{
	if(!data || !length) return 0;
    size_t j(0);
    for( ; j<length; ++j) {
        char const octet(data[j]);
        switch(m_state) {
        case WANTED_START:
            m_state = WANTED_CR;
            if(!is_white(octet) || m_headers.empty()) m_headers.push_back(std::string());//pocetak novog headera
            break;
        case WANTED_CR:
            if(octet == 0x0d) m_state = WANTED_LF;
            break;
        case WANTED_LF:
            if(octet == 0x0a) {
                m_state = WANTED_START; // line continued or start of new line expected
            } else {
                m_state = WANTED_CR; // cancel lf
            }
            break;
        case WANTED_NOTHING:
            std::wclog << "It should never be executed " << __FUNCTION__ << " for state WANTED_NOTHING" << std::endl;
            return 0;
        }
        m_headers.back().push_back(octet);
        if(m_headers.back() == HTTPDefs::STRING_CRLF) {
            m_headers.pop_back();
            m_state = WANTED_NOTHING;//end of message header
            for(size_t k(1); k< m_headers.size(); ++k) {
                std::string const& line(m_headers[k]);
                //DBGMSGAT("Puting to map line'" << line << "'");
                size_t const colon(line.find(':'));
                if(colon == std::string::npos) {
                    m_headers_map.add(line, std::string());
                    //DBGMSGAT("As field='" << line << "' and empty value");
                } else {
                    std::string value(line.substr(colon+1));
                    boost::algorithm::trim_if(value, &MsgHeadersParser::is_white_or_crlf);
                    m_headers_map.add(line.substr(0, colon), value);
                    //DBGMSGAT("As field='" << line.substr(0, colon) << "' value='" << value << "'");
                }
            }
            //DBGMSGAT("Finished headers parsing, normalizing");
            m_headers_map.normalize();
            //DBGMSGAT("Normalized headers map");
            break;
        }
    }
    DBGMSGAT("Finished header parsing cycle");
    return (j<length) ? j+1 : length;
}

size_t MsgChunksParser::parse(char const* data, size_t const length)
{
    if(!data || !length) return 0;
    size_t i(0);
    for( ; i<length; ++i) {
        char const octet(data[i]);
        switch(m_state) {
        case CHUNK_SIZE_WANTED_START:
            if(!is_hex(octet)) {//FAIL
                m_state = CHUNK_FINISHED;
                return 0;
            }
            m_tmp.clear();
            m_tmp.push_back(octet);
            m_state = CHUNK_SIZE_WANTED_CR;
            break;
        case CHUNK_SIZE_WANTED_CR:
            if(octet == 0x0d) {
                m_state = CHUNK_SIZE_WANTED_LF;
                break;
            }
            if(!is_hex(octet)) {//FAIL
                m_state = CHUNK_FINISHED;
                return 0;
            }
            m_tmp.push_back(octet);
            break;
        case CHUNK_SIZE_WANTED_LF:
            if(octet != 0x0a) {//FAIL
                m_state = CHUNK_FINISHED;
                return 0;
            }
            if(m_tmp[0] == '0') {//zero-sized chunk (OK) or leading zero (FAIL)
                m_state = CHUNK_FINISHED;
                return (m_tmp.size() > 1) ? 0 : i+1;
            }
            m_state = CHUNK_DATA_WANTED_START;
            try {
                m_remaining = boost::lexical_cast<size_t>(m_tmp);
            } catch (std::bad_cast& /*ex*/) {
                std::wclog << "Failed cast chunk length to value: '" << m_tmp << "'" << std::endl;
            }
            m_tmp.clear();
            if(m_body.size() + m_remaining > m_max_body_size) {//FAIL
                m_state = CHUNK_FINISHED;
                return 0;
            }
            break;
        case CHUNK_DATA_WANTED_START:
            m_state = CHUNK_DATA_WANTED_CR;
            m_body.push_back(octet);
            break;
        case CHUNK_DATA_WANTED_CR:
            if(octet == 0x0d) {
                m_state = CHUNK_DATA_WANTED_LF;
            } else {
                if(!m_remaining) {
                    m_state = CHUNK_FINISHED;
                    return 0;
                }
                --m_remaining;
                m_body.push_back(octet);
            }
            break;
        case CHUNK_DATA_WANTED_LF:
            if(octet == 0x0a) {
                m_state = CHUNK_SIZE_WANTED_START;
            } else {
                if(!m_remaining) {
                    m_state = CHUNK_FINISHED;
                    return 0;
                }
                --m_remaining;
                m_body.push_back(octet);
                m_state = CHUNK_DATA_WANTED_CR;//cancel LF
            }
            break;
        case CHUNK_FINISHED:
            std::wclog << "It should never be executed " << __FUNCTION__ << " for state CHUNK_FINISHED" << std::endl;
            return 0;
        }
    }
    return (i<length) ? i+1 : length;
}

size_t MsgBodyParser::parse(char const* data, size_t const length)
{
    if(!m_remaining) return 0;
    size_t length_actual(length);
    if(m_remaining>0 && length>size_t(m_remaining))
        length_actual = m_remaining;
    if(m_body.size() + length_actual > m_max_body_size)
        length_actual = m_max_body_size - m_body.size();
    m_body.insert(m_body.end(), data, data + length_actual);
    if(m_remaining>0)
        m_remaining -= length_actual;
    return length_actual;
}

void MsgBodyParser::set_finished(void)
{
    if(m_remaining > 0) {
        std::wclog << "Inconsistent use of body parser, setting as finished parser started for"
        " parsing until specified content length - not until connection closing" << std::endl;
    }
    m_remaining = 0;
}

bool MsgParser::is_header_only(void) const
{
    if(m_is_request) return false;
    StatusLine const* s(m_head_parser.get_status_line());
    if(!s) return false;
    return (m_response_for_requested_method == "HEAD"       // HEAD responses have no content
        || (s->code >= 100 && s->code <= 199)   // 1xx responses have no content
        || s->code == 204 || s->code == 205		// no content & reset content responses
        || s->code == 304							// not modified responses have no content
    );
}

size_t MsgParser::parse(char const* data, size_t const length)
{
    size_t remaining(length);
    char const* ptr(data);
    size_t consumed(0);
    while(remaining) {
        switch(m_state) {
        case WANTED_HEADERS:
            consumed = m_head_parser.parse(ptr, remaining);
            if(m_head_parser.is_finished()) {
                DBGMSGAT("Headers finished, preparing body parser");
                //odluka da li se parsira body, kao CHUNKS ili je FAILED
                int const body_more_bytes(m_head_parser.get_content_length());
                if(m_head_parser.is_chunked()) {
                    m_state = WANTED_CHUNKS;
                    m_chunked_parser.reset();
                    DBGMSGAT("Parsing as chunked body");
                } else if(is_header_only()) {
                    m_state = FINISHED;
                    m_body.clear();
                    DBGMSGAT("Message without body");
                } else if(body_more_bytes <= 0) {
                    DBGMSGAT("Not chunked body and no specified length, can continue only if it's response");
                    if(m_is_request) {
                        if(consumed < remaining) {
                            std::wclog << "Invalid request - must be chunked or length specified" << std::endl;
                            m_state = FAILED;
                            return 0;
                        } else {
                            m_state = FINISHED;
                            m_body.clear();
                            DBGMSGAT("Assuming message without body because it's end of bufer");
                        }
                    } else {
                        m_state = WANTED_BODY;
                        m_body_parser.reset(-1);
                        DBGMSGAT("Parsing response body as ordinary body without specified body length");
                    }
                } else {
                    m_state = WANTED_BODY;
                    m_body_parser.reset(body_more_bytes);
                    DBGMSGAT("Parsing ordinary body with specified body length: " << body_more_bytes);
                }
            } else if(consumed < remaining) {
                m_state = FAILED;
                return 0;
            }
            break;
        case WANTED_CHUNKS:
            consumed = m_chunked_parser.parse(ptr, remaining);
            if(m_chunked_parser.is_finished()) {
                m_state = FINISHED;
                m_chunked_parser.swap_body(m_body);
                DBGMSGAT("Finished chunked body");
            }
            break;
        case WANTED_BODY:
            consumed = m_body_parser.parse(ptr, remaining);
            if(m_body_parser.is_finished()) {
                m_state = FINISHED;
                m_body_parser.swap_body(m_body);
                DBGMSGAT("Finished ordinary body");
            }
            break;
        case FAILED:
            std::wclog << "It should never be executed " << __FUNCTION__ << " for state FAILED" << std::endl;
            return 0;
        case FINISHED:
            std::wclog << "It should never be executed " << __FUNCTION__ << " for state FINISHED" << std::endl;
        }
        if(!consumed) {
            m_state = FAILED;
            return 0;
        }
        remaining -= consumed;
        ptr += consumed;
        consumed = 0;

        if(m_state == FINISHED) return length - remaining;
    }
    return length;
}

std::string MsgParser::get_requested_method(void) const
{
    assert(m_is_request);
    RequestLine const* r(m_head_parser.get_request_line());
    return r ? r->method : std::string();
}

std::string MsgParser::get_requested_resource(void) const
{
    assert(m_is_request);
    RequestLine const* r(m_head_parser.get_request_line());
    return r ? r->resource_uri : std::string();
}

std::string MsgParser::get_requested_query(void) const
{
    assert(m_is_request);
    RequestLine const* r(m_head_parser.get_request_line());
    return r ? r->resource_query : std::string();
}

unsigned short MsgParser::get_major(void) const
{
    if(m_is_request) {
        RequestLine const* r(m_head_parser.get_request_line());
        return r ? r->version_major : 0;
    } else {
        StatusLine const* s(m_head_parser.get_status_line());
        return s ? s->version_major : 0;
    }
}

unsigned short MsgParser::get_minor(void) const
{
    if(m_is_request) {
        RequestLine const* r(m_head_parser.get_request_line());
        return r ? r->version_minor : 0;
    } else {
        StatusLine const* s(m_head_parser.get_status_line());
        return s ? s->version_minor : 0;
    }
}

unsigned int MsgParser::get_response_code(void) const
{
    assert(!m_is_request);
    StatusLine const* s(m_head_parser.get_status_line());
    return s ? s->code : 0;
}

std::string MsgParser::get_response_message(void) const
{
    assert(!m_is_request);
    StatusLine const* s(m_head_parser.get_status_line());
    return s ? s->message : std::string();
}

void MsgParser::set_finished(void)
{
    DBGMSGAT("Explicit setting body as finished");
    if(m_state == WANTED_BODY) {
        m_state = FINISHED;
        m_body_parser.set_finished();
        m_body_parser.swap_body(m_body);
        DBGMSGAT("Done.");
    } else if(m_state != FINISHED) {
        m_state = FAILED;
        DBGMSGAT("Failed, not parsing body, can't set as finished");
    }
}

}
}
