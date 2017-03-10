#ifndef MSG_SERIALIZER_H
#define MSG_SERIALIZER_H
#include "scarlet/http/HTTPDefs.h"
#include <list>


namespace scarlet {
namespace http {


class MsgSerializer {
    boost::shared_ptr<RequestLine>          m_rqline;
    boost::shared_ptr<StatusLine>           m_stline;
    HeadersMultimap                         m_headers;
    std::vector<boost::asio::const_buffer>  m_content_buffers;
    size_t									m_content_length;
	mutable std::list<std::string>			m_text_cache;

public:
    MsgSerializer(void) : m_content_length(0) { }

    void reset(bool reset_all = false)
    {
        if(reset_all) {
            m_rqline.reset();
            m_stline.reset();
            m_headers.clear();
            m_text_cache.clear();
        }
		m_content_buffers.clear();
		m_content_length = 0;
	}

    void set_first_line(StatusLine const& first_line)
    {
        m_rqline.reset();
        m_stline = boost::shared_ptr<StatusLine>(new StatusLine(first_line));
    }

    void set_first_line(RequestLine const& first_line)
    {
        m_rqline = boost::shared_ptr<RequestLine>(new RequestLine(first_line));
        m_stline.reset();
    }

    void set_header(std::string const& name, std::string const& value)
    {
        m_headers.set(name, value);
    }

	/** append payload content; the data written is not copied, and therefore must persist
	 * until the body is sent
	 * @param data the data to append to the payload content
	 */
	void append_nocopy(const std::string& data);

	/** append payload content;  the data written is not copied, and
	 * therefore must persist until the message has finished sending
	 * @param data points to the data to append to the payload content
	 * @param length the length, in bytes, of the data
	 */
	void append_nocopy(char const* data, size_t length);

    bool is_request(void) const;

    bool does_support_chunks(void) const;

    void header(std::vector<boost::asio::const_buffer>& buffer) const;
    size_t length(void) const { return m_content_length; }
    void body(std::vector<boost::asio::const_buffer>& buffer) const;
    void chunk(std::vector<boost::asio::const_buffer>& buffer) const;
    void final_chunk(std::vector<boost::asio::const_buffer>& buffer) const;
};


}
}

#endif //MSG_SERIALIZER_H
