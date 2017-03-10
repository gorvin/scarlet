#pragma once
#include <scarlet/http/HTTPDefs.h>
#include <boost/noncopyable.hpp>
#include <vector>

namespace scarlet {
namespace http {

class SvcHandler : public boost::noncopyable {
	explicit SvcHandler(void) = delete;
protected:
	/** creates a new HTTPServer object
	* @param scheduler the WorkScheduler that will be used to manage worker threads
	* @param endpoint TCP endpoint used to listen for new connections (see ASIO docs)
	*/
	SvcHandler(std::vector<std::string> const& resources);
	std::vector<std::string> const& getConfiguredResources(void) const { return m_resources; }
public:
	virtual ~SvcHandler() { }
	virtual bool parseSvcURI(MsgParserPtr request, boost::shared_ptr<httprequest_t> fmt_req, u8vector_t& domain) = 0;
	virtual boost::shared_ptr<httprequest_t> createFormattedRequestObject(void) = 0;
	virtual void handleResource(boost::shared_ptr<httprequest_t> fmt_req, boost::shared_ptr<httpresponse_t> response) = 0;
	virtual bool resolveUserImpl(std::string& pass, std::string const& name) = 0;
private:
	/// resources recognized by this HTTP service
	std::vector<std::string> m_resources;
};

}
}
