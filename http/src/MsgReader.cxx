#include "scarlet/http/MsgReader.h"
#include "bmu/Logger.h"
#include <boost/asio/placeholders.hpp>
#include <iostream>

namespace scarlet {
namespace http {

MsgReader::~MsgReader() 
{
	DBGMSGAT(" ~~~~~~~~~~~~~");
}

void MsgReader::receive(void)
{
	bool const bPipelined = m_tcp_conn->getPipelined();
    m_tcp_conn->setLifecycle(net::TCPConnection::LIFECYCLE_CLOSE);	// default to close the connection
	if (bPipelined) {
        DBGMSGAT("Loading saved read position on pipelined http connection");
		// there are pipelined messages available in the connection's read buffer
		char const* read_ptr(0);
		std::size_t bytes_left = m_tcp_conn->loadReadPosition(read_ptr);
		parse(read_ptr, bytes_left);
	} else {
        DBGMSGAT("Not pipelined");
		// no pipelined messages available in the read buffer -> read bytes from the socket
		readBytesWithTimeout();
	}
}

void MsgReader::consumeBytes(const boost::system::error_code& read_error, std::size_t bytes_read)
{
	// cancel read timer if operation didn't time-out
	DBGMSGAT("Canceling timer");
	m_timer.cancel();
	if (read_error) { // a read error occured
        m_tcp_conn->setLifecycle(net::TCPConnection::LIFECYCLE_CLOSE);// forcing the client to establish a new one
        if(m_http_msg->is_unacceptable_eof()) {
			DBGMSGAT(" READ ERROR unacceptable_eof");
			//if (getTotalBytesRead() > 0) { // only log if the parsing has already begun
                std::wclog << "HTTP " << ((m_msg_type == READ_AS_REQUEST) ? "request" : "response") << " parsing aborted ("
                    << ((read_error == boost::asio::error::operation_aborted)
                        ? "shutting down" //another thread is shutting-down the server
                        : read_error.message())
                    << ')' << " after read " << bytes_read << " bytes." << std::endl;
            //}
            m_finished_conn(m_tcp_conn);//m_tcp_conn->finish();//currently no way to propagate errors by triggering callback
        } else {
			DBGMSGAT(" READ END HTTP message with unknown length");
			m_http_msg->set_finished();
            finishedReading();// this is just a message with unknown content length
        }
	} else {
        DBGMSGAT("Read " << bytes_read << " bytes from HTTP " << ((m_msg_type == READ_AS_REQUEST) ? "request" : "response"));
        // set pointers for new HTTP header data to be consumed
        parse(m_tcp_conn->getReadBuffer().data(), bytes_read);
	}
}

void MsgReader::parse(char const* read_ptr, std::size_t bytes_read)
{
	size_t consumed(0);
    try {
        consumed = m_http_msg->parse(read_ptr, bytes_read);
    } catch(std::bad_alloc& ex) {
		_CRT_UNUSED(ex);
		DBGMSGAT(ex.what());
        consumed = 0;
    }
	if (consumed > 0)
		DBGMSGAT("Parsed " << consumed << " HTTP bytes");
	if(m_http_msg->is_finished()) { //if (result == true) { // finished reading HTTP message and it is valid
		DBGMSGAT("Successfully finished HTTP message parsing");
		if (m_http_msg->checkKeepAlive()) {// set the connection's lifecycle type
			if (consumed == bytes_read) {
				// the connection should be kept alive, but does not have pipelined messages
				m_tcp_conn->setLifecycle(net::TCPConnection::LIFECYCLE_KEEPALIVE);
                DBGMSGAT("Keeping alive connection");
			} else {
				// the connection has pipelined messages
				m_tcp_conn->setLifecycle(net::TCPConnection::LIFECYCLE_PIPELINED);
				//save the read position so that it can be retrieved by a new HTTP parser, which
				//will be created after the current message has been handled
				m_tcp_conn->saveReadPosition(read_ptr+consumed, bytes_read-consumed);
				DBGMSGAT("HTTP pipelined " << ((m_msg_type == READ_AS_REQUEST) ? "request (" : "response (")
                       << (bytes_read - consumed) << " bytes available)");
			}
		} else {
            DBGMSGAT("Closing connection");
			m_tcp_conn->setLifecycle(net::TCPConnection::LIFECYCLE_CLOSE);
		}
		DBGMSGAT("Calling HTTP message handler");
		// we have finished parsing the HTTP message
		finishedReading();
	} else if(m_http_msg->is_failed()) { //} else if (result == false) {
		DBGMSGAT("Failed HTTP message parsing");
		// the message is invalid or an error occured
		m_tcp_conn->setLifecycle(net::TCPConnection::LIFECYCLE_CLOSE);	// make sure it will get closed
		//m_http_msg->setIsValid(false);
		DBGMSGAT("Calling HTTP message handler on failed message");
		finishedReading();
	} else {
        DBGMSGAT("Not yet finished HTTP message parsing, need more bytes");
		readBytesWithTimeout();// not yet finished parsing the message -> read more data
	}
}

void MsgReader::timerCallback(const boost::system::error_code& ec)
{
	auto error_value = ec.value();
	if (boost::asio::error::operation_aborted != error_value) { // not canceled
		DBGMSGAT("Closing timed-out connection");
		if (m_tcp_conn->is_open())
			m_tcp_conn->close();
	} else {
		DBGMSGAT("Conection completly handled and timer was canceled.");
	}
}

void MsgReader::readBytesWithTimeout(void)
{
    DBGMSGAT("Async reading");
	if(m_read_timeout > 0) {
        m_timer.expires_from_now(boost::posix_time::seconds(m_read_timeout));
        m_timer.async_wait(boost::bind(&MsgReader::timerCallback, shared_from_this(), _1));
        DBGMSGAT("Setting up timer at " << m_read_timeout << " seconds");
	}
    m_tcp_conn->async_read_some(
        boost::bind(
            &MsgReader::consumeBytes,
            shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred
        )
    );
}

}
}
