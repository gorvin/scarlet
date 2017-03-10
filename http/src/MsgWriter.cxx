#include "scarlet/http/MsgWriter.h"
#include "bmu/Logger.h"
#include <boost/lexical_cast.hpp>
#include <boost/asio/placeholders.hpp>
#include <iostream>

namespace scarlet {
namespace http {

MsgWriter::~MsgWriter() 
{
	DBGMSGAT(" ~~~~~~~~~~~~~");
}

void MsgWriter::handleWrite(const boost::system::error_code& write_error, std::size_t bytes_written)
{
	m_tcp_conn->setSendingState(false);
    char const* const msgtype_str(m_is_request ? "request" : "response");
    if (write_error) { // encountered error sending response
        if(!m_is_request)
            m_tcp_conn->setLifecycle(net::TCPConnection::LIFECYCLE_CLOSE); // make sure it will get closed
		WARNCLOG("Unable to send HTTP " << msgtype_str << " (" << write_error.message() << ')');
	} else { // message sent OK
        if (m_sending_chunks) {
            m_msg->reset();
#ifndef NDEBUG
            std::wclog << "Sent HTTP " << msgtype_str << " chunk of " << bytes_written << " bytes" << std::endl;
#endif
        } else {
#ifndef NDEBUG
            std::wclog << "Sent HTTP " << msgtype_str << " of " << bytes_written << " bytes ("
                << (m_tcp_conn->getKeepAlive() ? "keeping alive)" : "closing)") << std::endl;
#endif
        }
    }
    //if (m_finished) m_finished();
    m_finished(m_tcp_conn);
}

void MsgWriter::prepare_message(bool try_chunked)
{
    m_msg->set_header(HTTPDefs::HEADER_CONNECTION, (m_tcp_conn->getKeepAlive() ? "Keep-Alive" : "close") );
    m_is_request = m_msg->is_request();
    if(try_chunked)
        m_sending_chunks = m_msg->does_support_chunks();
    if(m_sending_chunks) {
        m_msg->set_header(HTTPDefs::HEADER_TRANSFER_ENCODING, "chunked");
    } else if(m_msg->length() > 0) {
        try {
            m_msg->set_header(HTTPDefs::HEADER_CONTENT_LENGTH, boost::lexical_cast<std::string>(m_msg->length()));
        } catch (std::bad_cast& /*ex*/) {
            std::wclog << "Failed cast content length to string: '" << m_msg->length() << "'" << std::endl;
        }
    } else {
        assert(!m_is_request);
    }
}

bool MsgWriter::send_async_more(bool try_chunked, bool send_final_chunk)
{
#if 0
    // make sure that we did not lose the TCP connection
    if(!m_tcp_conn->is_open()) {
        m_tcp_conn->setLifecycle(TCPConnection::LIFECYCLE_CLOSE);// make sure it will get closed
        m_finished(m_tcp_conn);//m_tcp_conn->finish();
        return false;
    }
#endif
    // prepare the write buffers to be sent
    std::vector<boost::asio::const_buffer> buffer;
	if (!m_sent_headers) {// check if the HTTP headers have been sent yet
		prepare_message(try_chunked);
		m_msg->header(buffer);
		m_sent_headers = true;// only send the headers once
	}
    if(m_sending_chunks) {
        m_msg->chunk(buffer);
    } else {
        m_msg->body(buffer);
    }
	// prepare a zero-byte (final) chunk
	if (send_final_chunk && m_sending_chunks)
        m_msg->final_chunk(buffer);
	m_tcp_conn->setSendingState(true); // make sure not closed
    // send data in the write buffers
    m_tcp_conn->async_write(buffer, boost::bind(
            &MsgWriter::handleWrite, shared_from_this()
            , boost::asio::placeholders::error
            , boost::asio::placeholders::bytes_transferred
        )
    );
	// ne blokira, bice pozvana MsgWriter::handleWrite kad buffer bude poslan
    return true;
}

}
}
