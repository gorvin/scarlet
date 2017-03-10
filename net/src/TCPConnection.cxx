#include "scarlet/net/TCPConnection.h"
#include "IOSvcScheduler.h"

namespace scarlet {
namespace net {

TCPConnection::TCPConnection(IOSvcSchedulerPtr scheduler, const bool ssl_flag)
	: m_scheduler(scheduler)
	, m_ssl_socket(scheduler->getIOService(), scheduler->getSSLContext())
	, m_ssl_flag(ssl_flag)
	, m_read_position(0, 0)
	, m_lifecycle(LIFECYCLE_CLOSE)
	, m_sending(false)
{ }

TCPConnection::~TCPConnection()
{
	DBGMSGAT(" ~~~~~~~~~~~~~");
	close();
}

}
}
