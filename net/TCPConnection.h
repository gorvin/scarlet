#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <boost/noncopyable.hpp>
#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <bmu/Logger.h>

namespace scarlet {
namespace net {

class IOSvcScheduler;
typedef boost::shared_ptr<IOSvcScheduler> IOSvcSchedulerPtr;///<IOSvcScheduler pointer

class TCPConnection;
typedef boost::shared_ptr<TCPConnection> TCPConnectionPtr;///<TCPConnection pointer

/// TCPConnection: represents a single tcp connection
class TCPConnection : private boost::noncopyable {
public:
	/// data type for the connection's lifecycle state
	enum LifecycleType {
		LIFECYCLE_CLOSE,
		LIFECYCLE_KEEPALIVE,
		LIFECYCLE_PIPELINED,
	};
	enum { READ_BUFFER_SIZE = 8192 };///< size of the read buffer
    typedef boost::array<char, READ_BUFFER_SIZE>	ReadBuffer;///< I/O read buffer
	typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> SSLSocket;///< SSL socket connection
	typedef boost::asio::ssl::context						       SSLContext;///< SSL configuration context
private:
	/// data type for a read position bookmark
	typedef std::pair<const char*, std::size_t> ReadPosition;
	IOSvcSchedulerPtr m_scheduler;
	SSLSocket         m_ssl_socket;///< SSL connection socket
	bool              m_ssl_flag;///< true if the connection is encrypted using SSL
	ReadBuffer        m_read_buffer;///< buffer used for reading data from the TCP connection
	ReadPosition      m_read_position;///< saved read position bookmark
	LifecycleType     m_lifecycle;///< lifecycle state for the connection
	bool              m_sending;///< is the connection currently used for sending data
	TCPConnection(IOSvcSchedulerPtr scheduler, const bool ssl_flag);
public:
	~TCPConnection();
	/** creates new shared TCPConnection objects
	 * @param io_service asio service associated with the connection
	 * @param ssl_context asio ssl context associated with the connection
	 * @param ssl_flag if true then the connection will be encrypted using SSL
	 */
	static TCPConnectionPtr create(IOSvcSchedulerPtr scheduler, bool const ssl_flag)
	{
		return TCPConnectionPtr(new TCPConnection(scheduler, ssl_flag));
	}
	/** asynchronously accepts a new tcp connection
	 * @param tcp_acceptor object used to accept new connections
	 * @param handler called after a new connection has been accepted
	 * @see boost::asio::basic_socket_acceptor::async_accept()
	 */
	template <typename AcceptHandler>
	void async_accept(boost::asio::ip::tcp::acceptor& tcp_acceptor, AcceptHandler handler)
	{
		tcp_acceptor.async_accept(m_ssl_socket.lowest_layer(), handler);
	}
	/** asynchronously performs server-side SSL handshake for a new connection
	 * @param handler called after the ssl handshake has completed
	 * @see boost::asio::ssl::stream::async_handshake()
	 */
	template <typename SSLHandshakeHandler>
	void async_handshake_server(SSLHandshakeHandler handler)
	{
		m_ssl_socket.async_handshake(boost::asio::ssl::stream_base::server, handler);
	}
	/** asynchronously reads some data into the connection's read buffer
	 * @param handler called after the read operation has completed
	 * @see boost::asio::basic_stream_socket::async_read_some()
	 */
	template <typename ReadHandler>
	void async_read_some(ReadHandler handler) {
		if(m_ssl_flag)
			m_ssl_socket.async_read_some(boost::asio::buffer(m_read_buffer), handler);
		else
			m_ssl_socket.next_layer().async_read_some(boost::asio::buffer(m_read_buffer), handler);
	}
	/** asynchronously writes data to the connection
	 * @param buffers one or more buffers containing the data to be written
	 * @param handler called after the data has been written
	 * @see boost::asio::async_write()
	 */
	template <typename ConstBufferSequence, typename WriteHandler>
	void async_write(const ConstBufferSequence& buffers, WriteHandler handler)
	{
		if (m_ssl_flag)
			boost::asio::async_write(m_ssl_socket, buffers, handler);
		else
			boost::asio::async_write(m_ssl_socket.next_layer(), buffers, handler);
	}
	void setLifecycle(LifecycleType t) { m_lifecycle = t; }
	LifecycleType getLifecycle(void) const { return m_lifecycle; }
	void setSendingState(bool sending) { m_sending = sending; }
	bool getKeepAlive(void) const { return m_lifecycle != LIFECYCLE_CLOSE; }
	bool getPipelined(void) const { return m_lifecycle == LIFECYCLE_PIPELINED; }
	bool getSendingState(void) const { return m_sending; }
	/// returns the buffer used for reading data from the TCP connection
	ReadBuffer& getReadBuffer(void) { return m_read_buffer; }
	/** @param read_ptr points to the next character to be consumed in the read_buffer
	 * @param remaining bytes from read_ptr to the end of the read_buffer (last byte + 1)
	 */
	void saveReadPosition(const char *read_ptr, std::size_t remaining) {
		m_read_position = ReadPosition(read_ptr, remaining);
	}
	/** @param read_ptr points to the next character to be consumed in the read_buffer
	 * @return remaining bytes from read_ptr to the end of the read_buffer (last byte + 1)
	 */
	std::size_t loadReadPosition(const char *&read_ptr) const {
		read_ptr = m_read_position.first;
		return m_read_position.second;
	}
	/// returns true if the connection is currently open
	bool is_open(void) const { return m_ssl_socket.lowest_layer().is_open(); }
	/// closes the tcp socket and cancels any pending asynchronous operations
	void close(void) { 
		DBGMSGAT(" closing this connection is_open() = " << is_open());
		if(is_open()) m_ssl_socket.lowest_layer().close(); 
	}
	/// returns an ASIO endpoint for the client connection
	boost::asio::ip::tcp::endpoint getRemoteEndpoint(void) const
	{
		try {
			return m_ssl_socket.lowest_layer().remote_endpoint();
		} catch (boost::system::system_error&) {
		}
		return boost::asio::ip::tcp::endpoint();
	}
	boost::asio::ip::address getRemoteIp(void) const { return getRemoteEndpoint().address(); }
	unsigned short getRemotePort(void) const { return getRemoteEndpoint().port(); }
	boost::asio::io_service& getIOService(void) { return m_ssl_socket.lowest_layer().get_io_service(); }
	IOSvcSchedulerPtr getScheduler(void) const { return m_scheduler; }
};

}
}

#endif
