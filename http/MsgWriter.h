#ifndef MSG_WRITER_H
#define MSG_WRITER_H
#include <scarlet/http/MsgSerializer.h>
#include <scarlet/net/TCPConnection.h>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

namespace scarlet {
namespace http {

class MsgWriter : private boost::noncopyable, public boost::enable_shared_from_this<MsgWriter> {
	/// function called after the HTTP message has been sent
	typedef boost::function<void (net::TCPConnectionPtr)> FinishedConnHandler;
	/** protected constructor: only derived classes may create objects
	 * @param tcp_conn TCP connection used to send the message
	 * @param handler function called after the request has been sent
	 */
	MsgWriter(net::TCPConnectionPtr tcp_conn, MsgSerializerPtr msg, FinishedConnHandler handler)
    : m_tcp_conn(tcp_conn)
    , m_msg(msg)
    , m_sending_chunks(false)
    , m_sent_headers(false)
    , m_finished(handler)
	{ }
public:
	/** creates new MsgWriter objects
	 * @param tcp_conn TCP connection used to send the message
	 * @param handler function called after the message has been sent
	 * @return MsgWriterPtr shared pointer to the new writer object that was created
	 */
	static inline MsgWriterPtr create(net::TCPConnectionPtr tcp_conn, MsgSerializerPtr msg, FinishedConnHandler handler)
	{
		return MsgWriterPtr(new MsgWriter(tcp_conn, msg, handler));
	}
	/// default destructor
	virtual ~MsgWriter();
	/** Sends all data buffered as a single HTTP message (without chunking). Following a call to
     * this function, it is not thread safe to use your reference to the MsgWriter object.
	 */
	bool send_async(void) { return send_async_more(false); }
	/** Sends all data buffered as a single HTTP chunk and if argument is true also sends the final
	 * HTTP chunk. Following a call to this function, it is not thread safe to use your reference
	 * to the MsgWriter object until the handleWrite has been called.
	 */
	bool send_async_chunk(bool is_final = false) { return send_async_more(true, is_final); }
private:
	/** called after the message is sent
	 * @param write_error error status from the last write operation
	 * @param bytes_written number of bytes sent by the last write operation
	 */
	void handleWrite(const boost::system::error_code& write_error, std::size_t bytes_written);
	void prepare_message(bool try_chunked);
	/** sends all of the buffered data to the client
	 * @param send_final_chunk true if the final 0-byte chunk should be included
	 */
	bool send_async_more(bool try_chunked, bool is_final_chunk = false);
	/// The HTTP connection that we are writing the message to
	net::TCPConnectionPtr					m_tcp_conn;
    MsgSerializerPtr                        m_msg;
    bool                                    m_is_request;
	/// true if data is being sent to the client using multiple chunks
	bool									m_sending_chunks;
	/// true if the HTTP message headers have already been sent
	bool									m_sent_headers;
	/// function called after the HTTP message has been sent
	FinishedConnHandler						m_finished;
};

}
}

#endif //MSG_READER_H
