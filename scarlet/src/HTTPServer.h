#ifndef SCARLET_HTTP_SERVER_H
#define SCARLET_HTTP_SERVER_H
#include "Options.h"
#include <scarlet/net/TCPServer.h>
#include <scarlet/http/HTTPDefs.h>
#include <string>

namespace scarlet {

/// HTTPServer: a server that handles HTTP-XCAP connections
class HTTPServer : public net::TCPServer {
    /// resources recognized by this HTTP server
    std::vector<std::string> m_resources;
	/// maximum length for HTTP request payload content
	std::size_t m_max_content_length;

    static std::string get_configured_ssl(void);
	/** handles a new TCP connection
	 * @param tcp_conn the new TCP connection to handle
	 */
	void handleConnection(net::TCPConnectionPtr tcp_conn);
	/** handles a new HTTP request
	 * @param http_request the HTTP request to handle
	 * @param tcp_conn TCP connection containing a new request
	 */
	void handleRequest(http::MsgParserPtr http_request, net::TCPConnectionPtr tcp_conn);
    void initialize(void);

public:
	/// default destructor
	virtual ~HTTPServer() { if (isListening()) stop(); }

	/** creates a new HTTPServer object
	 * @param scheduler the WorkScheduler that will be used to manage worker threads
	 * @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
	 */
	HTTPServer(boost::asio::ip::tcp::endpoint const& endpoint);

	/// sets the maximum length for HTTP request payload content
	inline void setMaxContentLength(std::size_t n) { m_max_content_length = n; }
};

}

#endif //SCARLET_HTTP_SERVER_H
