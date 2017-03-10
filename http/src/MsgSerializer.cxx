#include "scarlet/http/MsgSerializer.h"
#include <stdio.h> // sprintf

namespace scarlet {
namespace http {

bool MsgSerializer::is_request(void) const
{
    assert((m_rqline && !m_stline) || (!m_rqline && m_stline));
    return m_rqline.get() != 0;
}

bool MsgSerializer::does_support_chunks(void) const
{
    assert((m_rqline && !m_stline) || (!m_rqline && m_stline));
    unsigned short vmajor(0);
    unsigned short vminor(0);
    if(m_rqline) {
        vmajor = m_rqline->version_major;
        vminor = m_rqline->version_minor;
    } else if(m_stline) {
        vmajor = m_stline->version_major;
        vminor = m_stline->version_minor;
    }
    return vmajor>1 || (vmajor == 1 && vminor >= 1);
}

void MsgSerializer::header(std::vector<boost::asio::const_buffer>& buffer) const
{
    assert((m_rqline && !m_stline) || (!m_rqline && m_stline));
    // add first line
    if(m_rqline) {
        m_text_cache.push_back(m_rqline->string());
    } else if(m_stline) {
        m_text_cache.push_back(m_stline->string());
    }
    buffer.push_back(boost::asio::buffer(m_text_cache.back()));
    buffer.push_back(boost::asio::buffer(HTTPDefs::STRING_CRLF));
    m_headers.serialize(buffer);// append HTTP headers
    buffer.push_back(boost::asio::buffer(HTTPDefs::STRING_CRLF));
}

void MsgSerializer::append_nocopy(const std::string& data)
{
    if(data.empty()) return;
    m_content_buffers.push_back(boost::asio::buffer(data));
    m_content_length += data.size();
}

void MsgSerializer::append_nocopy(char const* data, size_t length)
{
    if (!data || !length) return;
    m_content_buffers.push_back(boost::asio::buffer(data, length));
    m_content_length += length;
}

void MsgSerializer::body(std::vector<boost::asio::const_buffer>& buffer) const
{
    if(m_content_length == 0) return;
    buffer.insert(buffer.end(), m_content_buffers.begin(), m_content_buffers.end());
}

void MsgSerializer::chunk(std::vector<boost::asio::const_buffer>& buffer) const
{
    if(m_content_length == 0) return;
    char cast_buf[35];
    snprintf(cast_buf, _countof(cast_buf), "%lx", static_cast<long>(m_content_length));//chunk length in hex
    m_text_cache.push_back(cast_buf);
    buffer.push_back(boost::asio::buffer(m_text_cache.back()));
    buffer.push_back(boost::asio::buffer(HTTPDefs::STRING_CRLF));
    buffer.insert(buffer.end(), m_content_buffers.begin(), m_content_buffers.end());
    buffer.push_back(boost::asio::buffer(HTTPDefs::STRING_CRLF));
}

void MsgSerializer::final_chunk(std::vector<boost::asio::const_buffer>& buffer) const
{
    assert(m_content_length == 0);
    assert(m_content_buffers.size() == 0);
    m_text_cache.push_back("0");
    //zero-length chunk
    buffer.push_back(boost::asio::buffer(m_text_cache.back()));
    buffer.push_back(boost::asio::buffer(HTTPDefs::STRING_CRLF));
    //empty content chunk
    buffer.push_back(boost::asio::buffer(HTTPDefs::STRING_CRLF));
}

}
}
