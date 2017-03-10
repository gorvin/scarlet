#include "scarlet/net/TCPServer.h"
#include "IOSvcScheduler.h"
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/make_shared.hpp>

#ifndef _MSC_VER
#   include <sys/types.h>
#   include <unistd.h>
#   ifdef WIN32
int geteuid (void)
{
	return (0);
}

int seteuid (int u)
{
	(void)u;
	return (0);
}
#   endif
#endif

using boost::asio::ip::tcp;

namespace scarlet {
namespace net {

#ifndef _MSC_VER
/// AdminPermissions: obtains administrative rights for the process
class AdminPermissions {
	static boost::mutex					m_mutex;///< prevent corrupting user id
	boost::unique_lock<boost::mutex>	m_lock;
	boost::int16_t						m_user_id;///< user id before upgrading to administrator
	bool								m_has_rights;
public:
	/// will block if another thread has already obtained rights
	AdminPermissions(void)
    : m_lock(m_mutex)
    , m_user_id(geteuid())
    , m_has_rights(seteuid(0) == 0)
    {
        if(!m_has_rights) {
            LOGMSG(" Error " << "Unable to upgrade to administrative rights");
            m_lock.unlock();
        }
    }

	/// releases administrative rights
	~AdminPermissions()
	{
        if (m_has_rights) {
            if(seteuid(m_user_id) != 0)
                LOGMSG(" Error " << "Unable to release administrative rights");
            m_lock.unlock();
        }
    }
};

boost::mutex AdminPermissions::m_mutex;

#else
class AdminPermissions { };
#endif

// TCPServer member functions

TCPServer::TCPServer(const tcp::endpoint& endpoint, size_t concurency, size_t count_of_worker_threads)
	: m_asio_scheduler_group(boost::make_shared<IOSvcSchedulerGroup>(concurency, count_of_worker_threads))
	, m_endpoint(endpoint)
	, m_ssl_flag(false)
	, m_is_listening(false)
{ }

void TCPServer::start(void)
{
	// lock mutex for thread safety
	boost::mutex::scoped_lock server_lock(m_mutex);

	if (! m_is_listening) {
		LOGMSG("Starting server on port " << m_endpoint.port());

		beforeStarting();
		// get admin permissions in case we're binding to a privileged port
		AdminPermissions use_admin_rights;//(m_endpoint.port() < 1024);
		use_admin_rights; // unused variable

		// configure the acceptor service
		try {
			for (size_t i(0); i < m_asio_scheduler_group->count(); ++i) {
				auto& tcp_acceptor = m_asio_scheduler_group->getScheduler(i)->getAcceptor();
				tcp_acceptor.open(m_endpoint.protocol());
				// allow the acceptor to reuse the address (i.e. SO_REUSEADDR)
				// ...except when running not on Windows - see http://msdn.microsoft.com/en-us/library/ms740621%28VS.85%29.aspx
#ifdef _MSC_VER
				tcp_acceptor.set_option(tcp::acceptor::reuse_address(true));
#endif
				tcp_acceptor.set_option(boost::asio::socket_base::keep_alive(true));
				tcp_acceptor.bind(m_endpoint);
				if (0 == m_endpoint.port()) {
					// update the endpoint to reflect the port chosen by bind
					m_endpoint = tcp_acceptor.local_endpoint();
				}
				tcp_acceptor.listen(); // put acceptor in listen state
			}
		} 
		catch (std::exception& e) {
			LOGMSG(" Error " << "Unable to bind to port " << m_endpoint.port() << ": " << e.what());
			throw;
		}

		m_is_listening = true;

		// unlock the mutex since listen() requires its own lock
		server_lock.unlock();
		for (size_t i(0); i < m_asio_scheduler_group->count(); ++i) {
			auto scheduler = m_asio_scheduler_group->getScheduler(i);
			listen(scheduler);
		}
		// notify the thread scheduler that we need it now
		for (size_t i(0); i < m_asio_scheduler_group->count(); ++i) {
			m_asio_scheduler_group->getScheduler(i)->start(); // this executes asio.run() in scheduled threads
		}
	}
}

void TCPServer::stop(bool wait_until_finished)
{
	// lock mutex for thread safety
	boost::mutex::scoped_lock server_lock(m_mutex);

	if (m_is_listening) {
		LOGMSG("Shutting down server on port " << m_endpoint.port());
		m_is_listening = false;
		for (size_t i(0); i < m_asio_scheduler_group->count(); ++i) {
			auto& tcp_acceptor = m_asio_scheduler_group->getScheduler(i)->getAcceptor();
			// this terminates any connections waiting to be accepted
			tcp_acceptor.close();
		}
		if (!wait_until_finished) {
			// wait for all pending connections to complete
			// try to prune connections that didn't finish cleanly
			while (0 != pruneConnections()) { // if no more left, then we can stop waiting
				int csize = m_conn_pool.size();
				LOGMSG("Waiting for open connections to finish size =" << csize);
				// sleep for up to a quarter second to give open connections a chance to finish
				m_no_more_connections.timed_wait(server_lock, boost::posix_time::milliseconds(250));
			}
		}
		// notify the thread scheduler that we no longer need it
		for (size_t i(0); i < m_asio_scheduler_group->count(); ++i) {
			m_asio_scheduler_group->getScheduler(i)->stop();
		}
		// all done!
		afterStopping();
		m_server_has_stopped.notify_all();
	}
}

void TCPServer::join(void)
{
	boost::mutex::scoped_lock server_lock(m_mutex);
	while (m_is_listening) {
		// sleep until server_has_stopped condition is signaled
		m_server_has_stopped.wait(server_lock);
	}
}

void TCPServer::setSSLKeyFile(const std::string& pem_key_file)
{
	// configure server for SSL
	setSSLFlag(true);
	for (size_t i(0); i < m_asio_scheduler_group->count(); ++i) {
		auto& ssl_context = m_asio_scheduler_group->getScheduler(i)->getSSLContext();
		ssl_context.set_options(boost::asio::ssl::context::default_workarounds
			| boost::asio::ssl::context::no_sslv2
			| boost::asio::ssl::context::single_dh_use);
		ssl_context.use_certificate_file(pem_key_file, boost::asio::ssl::context::pem);
		ssl_context.use_private_key_file(pem_key_file, boost::asio::ssl::context::pem);
	}
}

void TCPServer::listen(IOSvcSchedulerPtr scheduler)
{
	// Single acceptor is used to wait for connections on one IO service at time.
	// On one acceptor connection can be accepted (handleAccept) from any of threads scheduled on this IO service
		
	//1. handleAccept se poziva za svaku novu prihvacenu konekciju
	//2. handleAccept se poziva iz jedne od niti koje izvrsavaju io_service::run i to iz one niti u kojoj
	// se u sklopu io_service::run obradi dati dogadjaj prihvatanja konekcije

	// lock mutex for thread safety
	boost::mutex::scoped_lock server_lock(m_mutex);
	if (m_is_listening) {
		// prune connections that finished uncleanly
		pruneConnections();
	
		// create a new TCP connection object
		TCPConnectionPtr new_conn(
			TCPConnection::create(scheduler, m_ssl_flag)
		);

		// keep track of the object in the server's connection pool
		m_conn_pool.insert(new_conn);

		// use the object to accept a new connection
		new_conn->async_accept(
			scheduler->getAcceptor()
			, boost::bind(&TCPServer::handleAccept, this, new_conn, boost::asio::placeholders::error)
		);
	}
}

void TCPServer::handleAccept(TCPConnectionPtr tcp_conn, boost::system::error_code const& accept_error)
{
	if (accept_error) {
		// an error occured while trying to a accept a new connection
		// this happens when the server is being shut down
		if (m_is_listening) {
			listen(tcp_conn->getScheduler());	// schedule acceptance of another connection
			WARNCLOG("Accept error on port " << m_endpoint.port() << ": " << accept_error.message());
		}
		finishConnection(tcp_conn);
	} else {
		// got a new TCP connection
		DBGMSGAT("New" << (m_ssl_flag/*tcp_conn->getSSLFlag()*/ ? " SSL " : " ")
					   << "connection on port " << m_endpoint.port());

		// schedule the acceptance of another new connection
		// (this returns immediately since it schedules it as an event)
		if (m_is_listening) 
			listen(tcp_conn->getScheduler());

		// handle the new connection
		if(m_ssl_flag) { //if (tcp_conn->getSSLFlag()) {
			tcp_conn->async_handshake_server(
				boost::bind(&TCPServer::handleSSLHandshake, this, tcp_conn, boost::asio::placeholders::error)
			);
		} else { // not SSL -> call the handler immediately
			handleConnection(tcp_conn); // ne blokira - samo scheduluje HTTP reader da se poziva kad stigne TCP paket
		}
	}
}

void TCPServer::handleSSLHandshake(TCPConnectionPtr tcp_conn, const boost::system::error_code& handshake_error)
{
	if (handshake_error) {
		// an error occured while trying to establish the SSL connection
		WARNCLOG("SSL handshake failed on port " << m_endpoint.port() << " (" << handshake_error.message() << ')');
		finishConnection(tcp_conn);
	} else {
		// handle the new connection
		DBGMSGAT("SSL handshake succeeded on port " << m_endpoint.port());
		handleConnection(tcp_conn); // ne blokira - samo scheduluje HTTP reader da se poziva kad stigne TCP paket
	}
}

void TCPServer::finishConnection(TCPConnectionPtr tcp_conn)
{
	boost::mutex::scoped_lock server_lock(m_mutex);
	DBGMSGAT(" with sending state = " << tcp_conn->getSendingState());
	if (!tcp_conn->getSendingState()) {
		if (m_is_listening && tcp_conn->getKeepAlive()) {
			// keep the connection alive
			handleConnection(tcp_conn); // ne blokira - samo scheduluje HTTP reader da se poziva kad stigne TCP paket
		} else {
			DBGMSGAT("Closing connection on port " << m_endpoint.port());
			// remove the connection from the server's management pool
			tcp_conn->close();
			m_conn_pool.erase(tcp_conn);

			// trigger the no more connections condition if we're waiting to stop
			if (!m_is_listening && m_conn_pool.empty())
				m_no_more_connections.notify_all();
		}
	}
}

std::size_t TCPServer::pruneConnections(void)
{
	// assumes that a server lock has already been acquired
	ConnectionPool::iterator conn_itr = m_conn_pool.begin();
	while (conn_itr != m_conn_pool.end()) {
		if (conn_itr->unique()) {
			WARNCLOG("Closing orphaned connection on port " << m_endpoint.port());
			ConnectionPool::iterator erase_itr = conn_itr;
			++conn_itr;
			(*erase_itr)->close();
			m_conn_pool.erase(erase_itr);
		} else {
			++conn_itr;
		}
	}

	// return the number of connections remaining
	return m_conn_pool.size();
}


}
}
