#include "HTTPServer.h"
#include "SvcXcap.h"
#include <scarlet/http/MsgReader.h>
#include <scarlet/http/RequestHandler.h>
#include <boost/make_shared.hpp>
#include <boost/filesystem.hpp>

namespace scarlet {

std::string HTTPServer::get_configured_ssl(void)
{
    std::string const ssl_fname(Options::instance().ssl_pem());
    if(ssl_fname.empty()) return std::string();
    return (boost::filesystem::path(Options::instance().topdir())/ssl_fname).string();
}

void HTTPServer::initialize(void)
{
    std::string ssl_filepath(get_configured_ssl());
    if(!ssl_filepath.empty()) {
		scarlet::net::TCPServer::setSSLFlag(true);
		scarlet::net::TCPServer::setSSLKeyFile(ssl_filepath);
    }
    m_resources = Options::instance().xcap_roots();
    std::wclog << "Server configured for these XCAP root URIs:\n";
    for(size_t i=0; i<m_resources.size(); ++i) {
        std::string port(std::to_string(scarlet::net::TCPServer::getEndpoint().port()));
        if(!port.empty() && port != "80") {
            m_resources[i].push_back(':');
            m_resources[i].append(port);
        }
        std::wclog << m_resources[i] << "\n";
    }
    std::wclog << std::flush;
    assert(!m_resources.empty());
}

/** creates a new HTTPServer object
 * @param scheduler the WorkScheduler that will be used to manage worker threads
 * @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
 */
HTTPServer::HTTPServer(const boost::asio::ip::tcp::endpoint& endpoint)
    : TCPServer(endpoint, Options::instance().concurrency(), 8)
    , m_max_content_length(scarlet::http::HTTPDefs::DEFAULT_MAX_BODY_SIZE)
{
    initialize();
}

void HTTPServer::handleConnection(scarlet::net::TCPConnectionPtr tcp_conn)
{
    DBGMSGAT("Creating http message reader on connection");
	scarlet::http::MsgReaderPtr reader_ptr;
	reader_ptr = scarlet::http::MsgReader::create(
        tcp_conn
        , boost::bind(&HTTPServer::handleRequest, this, _1, _2)
        , boost::bind(&HTTPServer::finishConnection, this, _1)
        , scarlet::http::MsgReader::READ_AS_REQUEST
        , m_max_content_length
    );
    DBGMSGAT("Starting reading");
	reader_ptr->receive(); 
	// receive() ne blokira, samo ce zadati da se poziva MsgReader::consumeBytes
	// iz threada u kom se primaju paketi na ovoj TCP konekciji
	
	// U consumeBytes se poziva MsgReader::parse za primljeni paket i
	// ako se ocekuje jos bajta nakon parsiranja onda se iz MsgReader::parse
	// opet scheduluje MsgReader::consumeBytes.
	
	// Kad se isparsira kompletan HTTP paket poziva se HTTPServer::handleRequest (preko funktora)
	// gdje se obradjuje XCAP zahtjev, formira i salje HTTP response
}

void HTTPServer::handleRequest(scarlet::http::MsgParserPtr http_request, scarlet::net::TCPConnectionPtr tcp_conn)
{
	//TODO: naci resurs koji pocinje kombinacijom server:port koju izvadis iz http_request - get_requested_resource()
	// ako postoji takav resurs koristi njemu dodijeljeni servis (handler) za obradu ovog request-a
    DBGMSGAT("Creating XCAP response task");
	static boost::filesystem::path xsddir;
	if (xsddir.empty()) {
		boost::filesystem::path tmp;
		tmp = boost::filesystem::path(Options::instance().topdir()) / Options::instance().subdir_xsd();
		if (boost::filesystem::exists(tmp))
			xsddir = tmp;
		if (xsddir.empty()) {
			tmp = boost::filesystem::path(Options::instance().topdir()) / ".." / Options::instance().subdir_xsd();
			if (boost::filesystem::exists(tmp))
				xsddir = tmp;
		}
		if (xsddir.empty()) {
			tmp = boost::filesystem::path(Options::instance().startdir()) / Options::instance().subdir_xsd();
			if (boost::filesystem::exists(tmp))
				xsddir = tmp;
		}
		if (xsddir.empty()) {
			tmp = boost::filesystem::path(Options::instance().startdir()) / ".." / Options::instance().subdir_xsd();
			if (boost::filesystem::exists(tmp))
				xsddir = tmp;
		}

		if (xsddir.empty()) {
			std::wclog << "Fatal error, cannot locate XSD dir " << Options::instance().subdir_xsd() << " in search path.\n"
				<< "Terminating server." << std::endl;
			std::terminate();//(FATAL)
		}

		xsddir = boost::filesystem::system_complete(xsddir);
	}
	static boost::shared_ptr<XcapResponseContext> xcacontext(boost::make_shared<XcapResponseContext>(
		Options::instance().locale()
		, xsddir.string()
		, Options::instance().namespace_schema()
		, Options::instance().domain()
		, Options::instance().storage_backend()
#if defined(WITH_BACKEND_POSTGRESQL)
		, Options::instance().connect_options()
#else
		, std::string()
#endif
		, Options::instance().topdir()
		, Options::instance().db_xtable()
		, Options::instance().db_utable()
		));

	// try to handle the request
	SvcXcapPtr xcasvc(SvcXcap::create(m_resources, xcacontext));
	scarlet::http::RequestHandlerPtr reqhandler(scarlet::http::RequestHandler::create(xcasvc));
	reqhandler->handleRequest(tcp_conn, boost::bind(&HTTPServer::finishConnection, this, _1), http_request);
}

}
