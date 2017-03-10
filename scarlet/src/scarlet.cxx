#include "Options.h"
#include <bmu/Logger.h>
#include "HTTPServer.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#ifndef WIN32
#   include <signal.h>
#endif

#if defined(_MSC_VER) && _MSC_VER>=19
#include <Windows.h>
// Warning: Hack!
// below is a stub for missing symbol in VS2015 UCRT used to return stdin/stdout/stderr pointers
// due to changes in UCRT libraries stdin/stdout/stderr compiled with older VC versions can't be linked:
// https://connect.microsoft.com/VisualStudio/feedback/details/1144980

// we hack it up to return the correct result depending on how it's called
extern "C" FILE * __cdecl __iob_func(void)
{
	//inspect the instrucion following the call
	LPBYTE retaddr = (LPBYTE)_ReturnAddress();
#ifndef _M_AMD64
	//if it's "add eax, 20h (83 C0 20)", the caller wants to get stdout, so return sdout-0x20
	//if it's "add eax, 40h (83 C0 40)", the caller wants to get stderr, so return sdout-0x40
	// in other cases assume and return stdin

	if (retaddr[0] != 0x83 || retaddr[1] != 0xC0)
		return stdin;
	switch (retaddr[2])
	{
	case 0x20: return (FILE*)((char*)stdout - 0x20);
	case 0x40: return (FILE*)((char*)stderr - 0x40);
	default: assert(0);
	}
#else
	// for x64: 48 83 C0 30 (add rax, 30h) or 48 83 C0 60 (add rax. 60h)
	if (retaddr[0] != 0x48 || retaddr[1] != 0x83 || retaddr[2] != 0xC0)
		return stdin;
	switch (retaddr[3])
	{
	case 0x30: return (FILE*)((char*)stdout - 0x30);
	case 0x60: return (FILE*)((char*)stderr - 0x60);
	default: assert(0);
	}
#endif
	assert(0);
	return 0;
}
#endif

/// ShutdownManager: used to manage shutdown for the main thread
class ShutdownManager {
public:
	// default constructor & destructor
	ShutdownManager(void) : m_shutdown_now(false) {}
	~ShutdownManager() {}

	/// signals the shutdown condition
	inline void shutdown(void) {
		boost::mutex::scoped_lock shutdown_lock(m_shutdown_mutex);
		m_shutdown_now = true;
		m_shutdown_cond.notify_all();
	}

	/// blocks until the shutdown condition has been signaled
	inline void wait(void) {
		boost::mutex::scoped_lock shutdown_lock(m_shutdown_mutex);
		while (!m_shutdown_now)
			m_shutdown_cond.wait(shutdown_lock);
	}

private:
	bool					m_shutdown_now;
	boost::mutex			m_shutdown_mutex;
	boost::condition		m_shutdown_cond;
};

/// static shutdown manager instance used to control shutdown of main()
static ShutdownManager	main_shutdown_manager;

/// signal handlers that trigger the shutdown manager
#ifdef WIN32
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
	std::wclog << "ctrl detected win" << std::endl;
	switch (ctrl_type) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		main_shutdown_manager.shutdown();
		return TRUE;
	default:
		return FALSE;
	}
}
#else
void handle_signal(int sig)
{
	std::wclog << "ctrl detected posix" << std::endl;
	main_shutdown_manager.shutdown();
}
#endif


unsigned short get_configured_port(void)
{
	unsigned short const port(scarlet::Options::instance().port());
	if (port != 0) return port;

	return scarlet::Options::instance().ssl_pem().empty() ? 8080 : 443;
}

int main(int argc, char* argv[])
{
	if (!scarlet::Options::instance().reset(argc, argv))
		return 0;

	// setup signal handler
#ifdef WIN32
	::SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
	::signal(SIGINT, handle_signal);
#endif

	try {
		bmu::LogsFactoryPtr logger_scope(bmu::LogsFactory::create());//initialize log system 
		logger_scope->setModifiers({ bmu::logmod_time }); // use simple configuration
		bmu::LogScopePtr lsroot(bmu::LogScope::create(nullptr));
		std::wclog << lsroot << std::endl;
#ifndef NDEBUG
		lsroot->setLoglevel(bmu::LDUMP);
#endif
		using boost::asio::ip::tcp;
		//TODO: iz config procitaj (IP:PORT, resource/xcap_domain) parove pa inicijalizuj
		// poseban ioservice za svaki IP:PORT
		scarlet::HTTPServer server(tcp::endpoint(tcp::v4(), get_configured_port()));
		server.start();
		main_shutdown_manager.wait();
	}
	catch (...) {
		std::wclog << "Exception encountered in server" << std::endl;
	}

	return 0;
}
