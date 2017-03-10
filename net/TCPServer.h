#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <scarlet/net/TCPConnection.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <set>

namespace scarlet {
namespace net {

class IOSvcSchedulerGroup;
typedef boost::shared_ptr<IOSvcSchedulerGroup> IOSvcSchedulerGroupPtr;///<IOSvcSchedulerGroup pointer

/// TCPServer: a multi-threaded, asynchronous TCP server
class TCPServer : private boost::noncopyable {
public:
	/// default destructor
	virtual ~TCPServer() { if (m_is_listening) stop(false); }
	/// starts listening for new connections
	void start(void);
	/** stops listening for new connections
	 * @param wait_until_finished if true, blocks until all pending connections have closed
	 */
	void stop(bool wait_until_finished = false);
	/// the calling thread will sleep until the server has stopped listening for connections
	void join(void);
	/** configures server for SSL using a PEM-encoded RSA private key file
	 * @param pem_key_file name of the file containing a PEM-encoded private key
	 */
	void setSSLKeyFile(const std::string& pem_key_file);
	/// returns tcp endpoint that the server listens for connections on
	inline const boost::asio::ip::tcp::endpoint& getEndpoint(void) const { return m_endpoint; }
	/// returns true if the server uses SSL to encrypt connections
	inline bool getSSLFlag(void) const { return m_ssl_flag; }
	/// sets value of SSL flag (true if the server uses SSL to encrypt connections)
	inline void setSSLFlag(bool b = true) { m_ssl_flag = b; }
	/// returns true if the server is listening for connections
	inline bool isListening(void) const { return m_is_listening; }
protected:
	/** protected constructor so that only derived objects may be created
	 * @param scheduler the WorkScheduler that will be used to manage worker threads
	 * @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
	 */
	TCPServer(boost::asio::ip::tcp::endpoint const& endpoint, size_t concurency, size_t count_of_worker_threads);
	/** handles a new TCP connection; derived classes SHOULD override this
	 * since the default behavior does nothing
	 * @param tcp_conn the new TCP connection to handle
	 */
	virtual void handleConnection(TCPConnectionPtr tcp_conn) {
		tcp_conn->setLifecycle(TCPConnection::LIFECYCLE_CLOSE);	// make sure it will get closed
		finishConnection(tcp_conn);//tcp_conn->finish();
	}
	/// called before the TCP server starts listening for new connections
	virtual void beforeStarting(void) {}
	/// called after the TCP server has stopped listing for new connections
	virtual void afterStopping(void) {}
protected:
	/// This will be called by TCPConnection::finish() after a server has
	/// finished handling a connection.  If the keep_alive flag is true,
	/// it will call handleConnection(); otherwise, it will close the
	/// connection and remove it from the server's management pool
	void finishConnection(TCPConnectionPtr tcp_conn);
private:
	/// handles a request to stop the server
	void handleStopRequest(void);
	/// listens for a new connection
	void listen(IOSvcSchedulerPtr scheduler);
	/** handles new connections (checks if there was an accept error)
	 * @param tcp_conn the new TCP connection (if no error occurred)
	 * @param accept_error true if an error occurred while accepting connections
	 */
	void handleAccept(TCPConnectionPtr tcp_conn, boost::system::error_code const& accept_error);
	/** handles new connections following an SSL handshake (checks for errors)
	 * @param tcp_conn the new TCP connection (if no error occurred)
	 * @param handshake_error true if an error occurred during the SSL handshake
	 */
	void handleSSLHandshake(TCPConnectionPtr tcp_conn, boost::system::error_code const& handshake_error);
    /// prunes orphaned connections that did not close cleanly
    /// and returns the remaining number of connections in the pool
    std::size_t pruneConnections(void);
	/// data type for a pool of TCP connections
	typedef std::set<TCPConnectionPtr>		ConnectionPool;
	/// reference to the active WorkScheduler object used to manage worker threads
	IOSvcSchedulerGroupPtr                  m_asio_scheduler_group;
	/// condition triggered when the server has stopped listening for connections
	boost::condition						m_server_has_stopped;
	/// condition triggered when the connection pool is empty
	boost::condition						m_no_more_connections;
	/// pool of active connections associated with this server
	ConnectionPool							m_conn_pool;
	/// tcp endpoint used to listen for new connections
	boost::asio::ip::tcp::endpoint			m_endpoint;
	/// true if the server uses SSL to encrypt connections
	bool									m_ssl_flag;
	/// set to true when the server is listening for new connections
	bool									m_is_listening;
	/// mutex to make class thread-safe
	mutable boost::mutex					m_mutex;
};

}
}

#endif
