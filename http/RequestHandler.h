#pragma once
#include <scarlet/net/TCPConnection.h>
#include <scarlet/http/HTTPDefs.h>
#include <scarlet/http/ResourceAuth.h>
#include <boost/thread/tss.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <vector>

namespace scarlet {
namespace http {

typedef boost::function<void(net::TCPConnectionPtr)> FinishedConnectionFn;
typedef boost::function<bool(MsgParserPtr, u8vector_t&)> ResourceLocatorFn;

class SvcHandler;
typedef boost::shared_ptr<SvcHandler> SvcHandlerPtr;
typedef boost::shared_ptr<ResourceAuth> ResourceAuthenticationPtr;
class RequestHandler;
typedef boost::shared_ptr<RequestHandler> RequestHandlerPtr;

class RequestHandler : public boost::enable_shared_from_this<RequestHandler> {
	explicit RequestHandler(void) = delete;
	void exceptions_handler(boost::shared_ptr<httpresponse_t> response);
	bool formatRequest(MsgParserPtr http_request, boost::shared_ptr<httprequest_t> fmt_request, boost::shared_ptr<httpresponse_t> response);
	void sendResponse(unsigned short ver_major, unsigned short ver_minor, net::TCPConnectionPtr tcp_conn, FinishedConnectionFn finishHandler
		, boost::shared_ptr<httpresponse_t> response, std::string const& challenge_domain);
	/** creates a new HTTPServer object
	* @param scheduler the WorkScheduler that will be used to manage worker threads
	* @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
	*/
	RequestHandler(SvcHandlerPtr svc_handler);
public:
	/// default destructor
	virtual ~RequestHandler() { }
	/** creates a new HTTPServer object
	* @param scheduler the WorkScheduler that will be used to manage worker threads
	* @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
	*/
	static RequestHandlerPtr create(SvcHandlerPtr svc_handler);
	/** handles a new HTTP request
	* @param http_request the HTTP request to handle
	* @param tcp_conn TCP connection containing a new request
	*/
	void handleRequest(net::TCPConnectionPtr tcp_conn, FinishedConnectionFn finishHandler, MsgParserPtr http_request);

	/** handles a new TCP connection
	* @param tcp_conn the new TCP connection to handle
	*/
	//void handleConnection(net::TCPConnectionPtr tcp_conn, FinishedConnectionFn finishHandler);
	/// sets the maximum length for HTTP request payload content
	inline void setMaxContentLength(std::size_t n) { m_max_content_length = n; }
private:
	SvcHandlerPtr             svc_handler;
	ResourceAuthenticationPtr auth;
	/// maximum length for HTTP request payload content
	std::size_t               m_max_content_length;
};

}
}
