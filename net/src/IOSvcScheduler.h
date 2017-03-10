#ifndef IO_SVC_SCHEDULER_H
#define IO_SVC_SCHEDULER_H

#include <bmu/Logger.h>
//#include <bmu/WorkScheduler.h>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

namespace scarlet {
namespace net {

/// IOSvcScheduler: manages thread pool for scheduling work on a single Boost.ASIO IO service 
class IOSvcScheduler /*: public bmu::WorkScheduler*/ {
	IOSvcScheduler(IOSvcScheduler const&) = delete;
	void operator=(IOSvcScheduler const&) = delete;
public:
	/// default number of worker threads in the thread pool
	enum { DEFAULT_NUM_THREADS = 8 };
	/// constructs a new IOSvcScheduler
	IOSvcScheduler(const boost::uint32_t num_threads = DEFAULT_NUM_THREADS);
	/// virtual destructor
	virtual ~IOSvcScheduler();

	/// returns an async I/O service used to schedule work
	boost::asio::io_service& getIOService(void) { return m_service; }

	/// returns an acceptor on this I/O service
	boost::asio::ip::tcp::acceptor& getAcceptor(void) { return m_acceptor; }

	/// returns an context used for SSL configuration on this I/O service
	boost::asio::ssl::context& getSSLContext(void) { return m_ssl_context; }

	/// Starts the thread scheduler
	void start(void);
	void stop(void);
private:
	/// processes work passed to the asio service & handles uncaught exceptions
	void processServiceWork(void);
	boost::mutex              m_mutex; ///< mutex to make class thread-safe
	typedef std::vector<boost::shared_ptr<boost::thread> >	ThreadPool;
	ThreadPool				        m_thread_pool; ///< pool of threads used to perform work
	boost::asio::io_service			m_service; ///< service used to manage async I/O events
	boost::shared_ptr<boost::asio::io_service::work> m_work;
	boost::asio::ip::tcp::acceptor  m_acceptor; ///< manages async TCP connections
	boost::asio::ssl::context       m_ssl_context; ///> context used for SSL configuration
	//boost::asio::deadline_timer		m_timer;/// timer used to periodically check for shutdown
};

typedef boost::shared_ptr<IOSvcScheduler> IOSvcSchedulerPtr;

/// IOSvcSchedulerGroup: uses a single IO service for each thread
class IOSvcSchedulerGroup {
public:
	/// constructs a new IOSvcSchedulerGroup
	IOSvcSchedulerGroup(boost::uint32_t nservices, boost::uint32_t num_threads = IOSvcScheduler::DEFAULT_NUM_THREADS);
	/// destructor
	~IOSvcSchedulerGroup();
	boost::uint32_t count(void) const 
	{ 
		return m_schedulers.size(); 
	}
	/// returns an async I/O service used to schedule work
	//boost::asio::io_service& getNextMemberIOService(boost::uint32_t& idx);
	/** returns an async I/O service used to schedule work (provides direct access to avoid locking when possible)
	* @param n integer number representing the service object
	*/
	IOSvcSchedulerPtr getScheduler(boost::uint32_t n) 
	{
		assert(n < m_schedulers.size());
		return m_schedulers[n];
	}
	//	/**
	//	 * schedules work to be performed by one of the pooled threads
	//	 *
	//	 * @param work_func work function to be executed
	//	 */
	//	virtual void post(boost::function<void(void)> work_func) {
	//		getIOService().post(work_func);
	//	}
private:
	/// mutex to make class thread-safe
	boost::mutex					m_mutex;
	/// true if the scheduler group is running
	bool							m_is_running;
	/// typedef for a pool of single service schedulers
	typedef std::vector<IOSvcSchedulerPtr>	scheduler_group;
	/// pool of single service schedulers used to perform work
	scheduler_group                 m_schedulers;
};

typedef boost::shared_ptr<IOSvcSchedulerGroup> IOSvcSchedulerGroupPtr;

}
}

#endif // IO_SVC_SCHEDULER_H
