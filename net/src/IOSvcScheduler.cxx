#include "IOSvcScheduler.h"
#include <boost/make_shared.hpp>

namespace scarlet {
namespace net {

IOSvcScheduler::IOSvcScheduler(const boost::uint32_t num_threads)
	: m_thread_pool(num_threads)
	, m_service()
	, m_work()
	, m_acceptor(m_service)
	, m_ssl_context(m_service, boost::asio::ssl::context::sslv23)
{
}

IOSvcScheduler::~IOSvcScheduler() 
{ 
	stop();
}

void IOSvcScheduler::processServiceWork(void) {
	LOGMSG(" Entered service worker loop");
	try {
		m_service.run();
	}
	catch (std::exception& e) {
		LOGMSG(" Error from asio handler " << e.what());
	}
	catch (...) {
		LOGMSG(" Error from asio handler " << "caught unrecognized exception");
	}
	LOGMSG(" Exiting from service worker loop");
}

void IOSvcScheduler::start(void)
{
	boost::mutex::scoped_lock scheduler_lock(m_mutex);
	LOGMSG("Starting thread scheduler");
	if (!m_work) {
		m_service.reset();
		// schedule a work item to make sure that the service doesn't complete
		m_work = boost::make_shared<boost::asio::io_service::work>(m_service);
		// start multiple threads to handle async tasks
		for (boost::uint32_t n = 0; n < m_thread_pool.size(); ++n) {
			//auto fnJob = boost::bind(&IOSvcScheduler::processServiceWork, this, boost::ref(m_service));
			auto fnJob = boost::bind(&IOSvcScheduler::processServiceWork, this);
			m_thread_pool[n] = boost::make_shared<boost::thread>(fnJob);
		}
	}
}

void IOSvcScheduler::stop(void)
{
	LOGMSG("Shutting down thread scheduler");
	boost::mutex::scoped_lock scheduler_lock(m_mutex);
	if (m_work) {
		m_service.stop();
		// wait until all threads in the pool have stopped, but skip current
		boost::thread current_thread;
		for (ThreadPool::iterator i(m_thread_pool.begin()); i != m_thread_pool.end(); ++i) {
			if (**i != current_thread) {
				(*i)->join();
				i->reset();
			}
		}
		m_service.reset();
	}
	m_work.reset();
	LOGMSG("The thread scheduler has shutdown");
}


IOSvcSchedulerGroup::IOSvcSchedulerGroup(boost::uint32_t nservices, boost::uint32_t num_threads)
	: /*m_nservices(nservices), m_next_service(nservices),*/ m_schedulers(), m_is_running(false)
{
	DBGMSGAT("");
	// make sure there are enough services initialized
	while (m_schedulers.size() < nservices) {
		m_schedulers.push_back(boost::make_shared<IOSvcScheduler>(num_threads));
	}
}

IOSvcSchedulerGroup::~IOSvcSchedulerGroup() 
{ 
}

}
}