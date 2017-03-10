#ifndef MSG_READER_H
#define MSG_READER_H
#include <scarlet/http/MsgParser.h>
#include <scarlet/net/TCPConnection.h>
#include <boost/asio/deadline_timer.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

namespace scarlet {
namespace http {

class MsgReader : public boost::enable_shared_from_this<MsgReader> {
	/// function called after the HTTP message has been parsed
	typedef boost::function<void(boost::shared_ptr<MsgParser>, net::TCPConnectionPtr)>	FinishedMsgHandler;
	typedef boost::function<void(net::TCPConnectionPtr)>	FinishedConnHandler;
public:
	virtual ~MsgReader();
	/// Incrementally reads & parses the HTTP message
	void receive(void);
	/// sets the maximum number of seconds for read operations
	inline void setTimeout(boost::uint32_t seconds) { m_read_timeout = seconds; }
    enum msg_type_e {
        READ_AS_REQUEST,
        READ_AS_RESPONSE,
    };
	static inline MsgReaderPtr create(
		net::TCPConnectionPtr tcp_conn
        , FinishedMsgHandler msg_handler
        , FinishedConnHandler conn_handler
        , msg_type_e type
        , size_t max_body_size = HTTPDefs::DEFAULT_MAX_BODY_SIZE
        , std::string const& response_for_requested_method = std::string()
    )
    {
        return MsgReaderPtr(new MsgReader(
            tcp_conn, msg_handler, conn_handler, type, max_body_size, response_for_requested_method
        ));
    }
private:
	/** private constructor: only static function MsgReader::create can create objects
	 * @param type if READ_AS_REQUEST, the message is parsed as an HTTP request;
	 *             if READ_AS_RESPONSE, the message is parsed as an HTTP response
	 * @param tcp_conn TCP connection containing a new message to parse
	 */
	MsgReader(
		net::TCPConnectionPtr tcp_conn
        , FinishedMsgHandler msg_handler
        , FinishedConnHandler conn_handler
        , msg_type_e type
        , size_t max_body_size = HTTPDefs::DEFAULT_MAX_BODY_SIZE
        , std::string const& response_for_requested_method = std::string()
    )
    : m_tcp_conn(tcp_conn)
    , m_read_timeout(DEFAULT_READ_TIMEOUT)
    , m_msg_type(type)
    , m_http_msg(new MsgParser(max_body_size))
    , m_timer(tcp_conn->getIOService())
    , m_finished(msg_handler)
    , m_finished_conn(conn_handler)
    {
        m_http_msg->reset(type == READ_AS_REQUEST, response_for_requested_method);
        //m_http_msg->setRemoteIp(tcp_conn->getRemoteIp());
    }
	/**
	 * Consumes bytes that have been read using an HTTP parser
	 *
	 * @param read_error error status from the last read operation
	 * @param bytes_read number of bytes consumed by the last read operation
	 */
	void consumeBytes(const boost::system::error_code& read_error, std::size_t bytes_read);
    /// Consumes bytes that have been read using an HTTP parser
    void parse(char const* read_ptr, std::size_t bytes_read);
    /// Called after we have finished reading/parsing the HTTP message
    void finishedReading(void) { m_finished(m_http_msg, m_tcp_conn); }
	/// reads more bytes for parsing, with timeout support
	void readBytesWithTimeout(void);
	/** Callback handler for the deadline timer
	 * @param ec deadline timer error status code
	 */
	void timerCallback(const boost::system::error_code& ec);
    static const std::size_t                DEFAULT_MAX_TRANSFER_SIZE = 65536;
	/// default maximum number of seconds for read operations
	static const boost::uint32_t			DEFAULT_READ_TIMEOUT = 3;// 10;
	/// The HTTP connection that has a new HTTP message to parse
	net::TCPConnectionPtr					m_tcp_conn;
	/// maximum number of seconds for read operations
	boost::uint32_t							m_read_timeout;
    msg_type_e                              m_msg_type;
	/// The new HTTP message container being created
	MsgParserPtr            m_http_msg;
	/// deadline timer used to timeout TCP operations
	boost::asio::deadline_timer				m_timer;
    /// function called after the HTTP message has been parsed
	FinishedMsgHandler m_finished;
	FinishedConnHandler m_finished_conn;
};

}
}

#endif //MSG_READER_H
