#include "SvcXcap.h"
#include <scarlet/http/MsgReader.h>
#include "scarlet/xcap/xcadefs.h"
#include "scarlet/xcap/XCAccess.h"
#include "scarlet/xcap/XCAccessMgr.h"
#include "scarlet/xcap/URIParser.h"
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

boost::thread_specific_ptr<scarlet::xcap::XCAccessMgr> XcapResponseContext::amgr;
boost::thread_specific_ptr<scarlet::xcap::URIParser>   XcapResponseContext::xuriparser;
boost::shared_ptr<scarlet::xcap::Storage>              XcapResponseContext::storage;

XcapResponseContext::XcapResponseContext(std::string const& locale, std::string const& xsd_dir
	, std::map<std::string, std::string> const& xsdmap, std::string const& default_domain
	, std::string const& bkend, std::string const& db_options, std::string const& storage_dir
	, std::string const& db_xtable, std::string const& db_utable)
	: locale(locale)
	, xsd_dir(xsd_dir)
	, xsdmap(xsdmap)
	, default_domain(default_domain)
{
	//Allow only one instance of this type
	BOOST_ASSERT(!amgr.get());
	BOOST_ASSERT(!xuriparser.get());
	BOOST_ASSERT(!storage);
	XcapResponseContext::storage = XcapResponseContext::create_storage(bkend, db_options, storage_dir, db_xtable, db_utable);
}

boost::shared_ptr<scarlet::xcap::Storage> XcapResponseContext::getStorage(void) const
{ 
	return storage;
}

boost::thread_specific_ptr<scarlet::xcap::XCAccessMgr> const& XcapResponseContext::getManager(void)
{ 
	if (!amgr.get()) // thread specific instance
		amgr.reset(new scarlet::xcap::XCAccessMgr(locale, xsd_dir, xsdmap, default_domain, storage));
	return amgr;
}

boost::thread_specific_ptr<scarlet::xcap::URIParser> const& XcapResponseContext::getURIParser(void)
{ 
	if (!xuriparser.get()) // thread specific instance
		xuriparser.reset(new scarlet::xcap::URIParser(default_domain));
	return xuriparser;
}

boost::shared_ptr<scarlet::xcap::Storage> XcapResponseContext::create_storage(
	std::string const& bkend, std::string const& db_options, std::string const& storage_dir, std::string const& db_xtable, std::string const& db_utable)
{
	try {
		if (bkend.empty() || bkend == "filesystem")
			return boost::shared_ptr<scarlet::xcap::Storage>(
				new scarlet::xcap::StorageFilesystem(storage_dir));
#if defined(WITH_BACKEND_POSTGRESQL)
		else if (bkend == "postgresql")
			return boost::shared_ptr<scarlet::xcap::Storage>(
				new scarlet::xcap::StoragePostgreSql(db_options)); //("host=/tmp dbname=postgres");
#endif
#if defined(WITH_BACKEND_SQLITE3)
		else if (bkend == "sqlite3")
			return boost::shared_ptr<scarlet::xcap::Storage>(
				new scarlet::xcap::StorageSqlite3(boost::filesystem::system_complete(storage_dir) / "xca.sqlite", db_xtable, db_utable));
#endif
		std::wclog << "Fatal error, unknown storage backend type " << bkend << "\n"
			<< "Expecting one of values: filesystem, posgresql or sqlite3.\n"
			<< "Terminating server." << std::endl;
		std::terminate();//(FATAL)
	}
	catch (scarlet::xcap::storage_error& e) {
		std::string const* msg(boost::get_error_info<bmu::errinfo_message>(e));
		if (msg)
			std::wclog << *msg << std::endl;
		std::terminate();//(FATAL)
	}
	catch (std::bad_alloc& e) {
		std::wclog << e.what() << std::endl;
		std::terminate();//(FATAL)
	}
}

/** creates a new SvcXcap object
* @param resources all resources handled by this service
*/
SvcXcap::SvcXcap(std::vector<std::string> const& resources, boost::shared_ptr<XcapResponseContext> context)
	: scarlet::http::SvcHandler(resources)
	, context(context)
{
}

SvcXcapPtr SvcXcap::create(std::vector<std::string> const& resources, boost::shared_ptr<XcapResponseContext> context)
{
	return SvcXcapPtr(new SvcXcap(resources, context));
}

struct xcarequest_t : public scarlet::http::httprequest_t {
	scarlet::xcap::xcapuri_t uri;
};

boost::shared_ptr<scarlet::http::httprequest_t> SvcXcap::createFormattedRequestObject(void)
{
	return boost::make_shared<xcarequest_t>();
}

bool SvcXcap::parseSvcURI(scarlet::http::MsgParserPtr request, boost::shared_ptr<scarlet::http::httprequest_t> req, bmu::u8vector_t& domain)
{
	scarlet::xcap::xcapuri_t& xuri(boost::static_pointer_cast<xcarequest_t>(req)->uri);
	//U primjeni HTTP za web prakticno svi klijenti stavljaju relativni path u prvi red zahtjeva
	//(trazeni resurs) a host dio URLa (pocetak bez http[s]:/) stavljaju u 'Host' zaglavlje zahtjeva
	//U HTTP 1.1 cak se zahtijeva da se koristi Host - to vazi u opstem slucaju a ne samo za web
	if (!context->getURIParser()->reset(xuri.docpath, xuri.npath, domain, request->get_requested_resource())) {
		DBGMSGAT("Bad document or node selector");
		return false; //bad document or node selector
	}
	if (xuri.docpath.root.empty()) {
		std::string hdrhost(request->get_header(scarlet::http::HTTPDefs::HEADER_HOST));
		xuri.docpath.root.assign(hdrhost.begin(), hdrhost.end());
	}
	if (!context->getURIParser()->reset(xuri.prefixes, request->get_requested_query())) {
		DBGMSGAT("Bad URL query part");
		return false; //bad query
	}
	//If any names are present that use a namespace prefix not defined in an xmlns() in the query
	//component of URI, reject the request with a 400 response->
	for (size_t i = 0; i < xuri.npath.size(); ++i) {
		scarlet::xcap::u8vector_t const& prefix(xuri.npath[i].prefix);
		if (!prefix.empty() && xuri.prefixes.find(prefix) == xuri.prefixes.end()) {
			DBGMSGAT("Found prefix in node selector without bindings in query");
			return false;
		}
	}
	return true;
}

void SvcXcap::exceptions_handler(void)
{
	try {
		throw;
	}
	catch (scarlet::xcap::storage_error& e) {
		std::wclog << boost::diagnostic_information(e) << std::endl;
		std::terminate();//(FATAL)
	}
	catch (std::bad_alloc& e) {
		std::wclog << e.what() << std::endl;
		std::terminate();//(FATAL)
	}
	catch (boost::exception& e) {
		// recover gracefully from other exceptions thrown
		std::wclog << boost::diagnostic_information(e) << std::endl;
	}
	catch (std::exception& e) {
		// recover gracefully from other exceptions thrown
		std::wclog << e.what() << std::endl;
	}
}

class scarlet_xcacontext_t : public scarlet::xcap::xcacontext_t {
	scarlet_xcacontext_t(void) = delete;
public:
	scarlet_xcacontext_t(boost::shared_ptr<xcarequest_t> req, boost::shared_ptr<scarlet::http::httpresponse_t> rsp)
		: req(req)
		, rsp(rsp)
	{ }
private:
	//request handling part
	std::string const& rqUsername(void) const { return req->username; }
	std::string const& rqDomain(void) const { return req->domain; }
	scarlet::xcap::xcapuri_t const& rqUri(void) const { return req->uri; }
	std::vector<std::string> const& rqIfEtags(void) const { return req->ifetags; }
	std::vector<std::string> const& rqIfNoEtags(void) const { return req->ifnoetags; }
	method_e rqMethod(void) const;
	scarlet::xcap::rawcontent_t const& rqBody(void) const { return req->body; }
	std::string const& rqMime(void) const { return req->mime; }
	//response handling part
	void reCode(scarlet::xcap::response_code_e const& rc);
	scarlet::xcap::u8vector_t& reBodyOut(void) { return rsp->body; }
	std::string& reEtagOut(void) { return rsp->etag; }
	std::string& reMimeOut(void) { return rsp->mime; }
	std::map<std::string, std::string>& reExtraHeadersOut(void) { return rsp->extra_hdrs; }

	boost::shared_ptr<xcarequest_t>                  req;
	boost::shared_ptr<scarlet::http::httpresponse_t> rsp;
};

scarlet_xcacontext_t::method_e scarlet_xcacontext_t::rqMethod(void) const 
{ 
	switch (req->method)
	{
	case scarlet::http::httprequest_t::RQ_METHOD_GET: 
		return xcacontext_t::RQ_METHOD_GET;
	case scarlet::http::httprequest_t::RQ_METHOD_PUT: 
		return xcacontext_t::RQ_METHOD_PUT;
	case scarlet::http::httprequest_t::RQ_METHOD_DELETE: 
		return xcacontext_t::RQ_METHOD_DELETE;
	}
	return xcacontext_t::RQ_METHOD_GET;
}

void scarlet_xcacontext_t::reCode(scarlet::xcap::response_code_e const& rc)
{
	using namespace ::scarlet::http;
	using namespace ::scarlet::xcap;
	switch (rc)
	{
	case XCAP_OK: rsp->code = HTTP_OK; return;
	case XCAP_OK_CREATED: rsp->code = HTTP_OK_CREATED; return;
	case XCAP_FAIL_BAD_REQUEST: rsp->code = HTTP_FAIL_BAD_REQUEST; return;
	case XCAP_FAIL_AUTHORIZATION: rsp->code = HTTP_FAIL_AUTHORIZATION; return;
	case XCAP_FAIL_NOT_FOUND: rsp->code = HTTP_FAIL_NOT_FOUND; return;
	case XCAP_FAIL_NOT_ALLOWED: rsp->code = HTTP_FAIL_NOT_ALLOWED; return;
	case XCAP_FAIL_CONSTRAINTS: rsp->code = HTTP_FAIL_CONSTRAINTS; return;
	case XCAP_FAIL_IF_PERFORM: rsp->code = HTTP_FAIL_IF_PERFORM; return;
	case XCAP_FAIL_MIME: rsp->code = HTTP_FAIL_MIME; return;
	case XCAP_ERROR_INTERNAL: rsp->code = HTTP_ERROR_INTERNAL; return;
	}
}

void SvcXcap::handleResource(boost::shared_ptr<scarlet::http::httprequest_t> req, boost::shared_ptr<scarlet::http::httpresponse_t> response)
{
	boost::shared_ptr<xcarequest_t> xreq(boost::static_pointer_cast<xcarequest_t>(req));
	//CHECK: ovaj servis se ne bi trebao uopste pozvati ako ne rukuje ovim resursom ???
	std::string const root_requested(xreq->uri.docpath.root.begin(), xreq->uri.docpath.root.end());
	std::vector<std::string> const& confRes(SvcHandler::getConfiguredResources());
	if (std::find(confRes.begin(), confRes.end(), root_requested) == confRes.end()) {
		DBGMSGAT("Requested XCAP root URI '" << root_requested.c_str() << "' is not served by this server");
		response->code = scarlet::http::HTTP_FAIL_NOT_FOUND;
		return;
	}
	boost::shared_ptr<scarlet::xcap::XCAccess> app(context->getManager()->find(bmu::utf8_string(xreq->uri.docpath.auid)));
	if (!app) {
		DBGMSGAT("Not found requested XCAP auid");
		response->code = scarlet::http::HTTP_FAIL_NOT_FOUND;// 404 je i za Application not yet implemented!!!
		return;
	}
	DBGMSGAT("Found request handler for HTTP resource (XCAP root): " << xreq->domain << "/" << printable(xreq->uri.docpath) /*<< request->get_requested_resource()*/);
	try {
		app->handle_request(scarlet_xcacontext_t(xreq, response));
	}
	catch (...) {
		SvcXcap::exceptions_handler();
		return;
	}
}

bool SvcXcap::resolveUserImpl(std::string& pass, std::string const& name)
{
	return 0 == context->getStorage()->user(pass, name);
}
