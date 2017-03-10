#pragma once
#include "scarlet/xcap/Storage.h"
#include <scarlet/http/SvcHandler.h>
#include <boost/thread/tss.hpp>
#include <vector>

namespace scarlet {
namespace xcap {
class XCAccessMgr;
class URIParser;
}
}

class XcapResponseContext : public boost::noncopyable {
	explicit XcapResponseContext(void) = delete;
public:
	// Options::instance().storage_backend()
	XcapResponseContext(std::string const& locale, std::string const& xsd_dir
		, std::map<std::string, std::string> const& xsdmap, std::string const& default_domain
		, std::string const& bkend, std::string const& db_options, std::string const& storage_dir
		, std::string const& db_xtable, std::string const& db_utable);
	boost::shared_ptr<scarlet::xcap::Storage> getStorage(void) const;
	boost::thread_specific_ptr<scarlet::xcap::XCAccessMgr> const& getManager(void);
	boost::thread_specific_ptr<scarlet::xcap::URIParser> const& getURIParser(void);
private:
	static boost::shared_ptr<scarlet::xcap::Storage> create_storage(
		std::string const& bkend, std::string const& db_options, std::string const& storage_dir
		, std::string const& db_xtable, std::string const& db_utable);
	std::string const                        locale;
	std::string const                        xsd_dir;
	std::map<std::string, std::string> const xsdmap;
	std::string const                        default_domain;
	static boost::thread_specific_ptr<scarlet::xcap::XCAccessMgr> amgr;
	static boost::thread_specific_ptr<scarlet::xcap::URIParser>   xuriparser;
	static boost::shared_ptr<scarlet::xcap::Storage>              storage;
};

class SvcXcap;
typedef boost::shared_ptr<SvcXcap> SvcXcapPtr;

/// SvcXcap: a service that handles HTTP-XCAP requests
class SvcXcap : public scarlet::http::SvcHandler {
	explicit SvcXcap(void) = delete;
	static void exceptions_handler(void);
	virtual bool parseSvcURI(scarlet::http::MsgParserPtr request, boost::shared_ptr<scarlet::http::httprequest_t> fmt_req, scarlet::xcap::u8vector_t& domain);
	virtual boost::shared_ptr<scarlet::http::httprequest_t> createFormattedRequestObject(void);
	virtual void handleResource(boost::shared_ptr<scarlet::http::httprequest_t> fmt_req, boost::shared_ptr<scarlet::http::httpresponse_t> response);
	virtual bool resolveUserImpl(std::string& pass, std::string const& name);
	/** creates a new SvcXcap object */
	SvcXcap(std::vector<std::string> const& resources, boost::shared_ptr<XcapResponseContext> context);
public:
	static SvcXcapPtr create(std::vector<std::string> const&, boost::shared_ptr<XcapResponseContext>);
	/// default destructor
	virtual ~SvcXcap() { }
private:
	boost::shared_ptr<XcapResponseContext> context;
};
