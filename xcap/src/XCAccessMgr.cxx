#include "scarlet/xcap/XCAccessMgr.h"
#include "usages/XCACapabilities.h"
#include "usages/XCAResourceLists.h"

namespace scarlet {
namespace xcap {

app_usage_t prepare_info_pidf(void)
{
    app_usage_t tmp = {
        "pidf-manipulation",
        "application/pidf+xml",
        "urn:ietf:params:xml:ns:pidf",
        vstrings_t(),
        "index",
        false,
        vstrings_t()
    };

    return tmp;
}

//PIDF manipulation (4827) does not define aditional constraints or resource interdependencies
class XCAPIDFManipulation : public XCAccess {
	XCAPIDFManipulation(void) = delete;
public:
	XCAPIDFManipulation(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap)
		: XCAccess(xerces_scope, xsd_dir, xsdmap, prepare_info_pidf())
     { }
};

app_usage_t prepare_info_presrules(void)
{
    app_usage_t tmp = {
        "pres-rules",
        "application/auth-policy+xml",
        "urn:ietf:params:xml:ns:pres-rules",
        vstrings_t(),//TODO: other "urn:ietf:params:xml:ns:common-policy"
        "index",
        false,
        vstrings_t()
    };

    return tmp;
}

//Pres Rules (5025) does not define aditional constraints or resource interdependencies
//NOTE: U OMA varijanti postoje dodatne provjere
//, ime dokumenta je 'pres-rules'
//, auid je org.openmobilealliance.pres-rules
class XCAPresRules : public XCAccess {
	XCAPresRules(void) = delete;
public:
    XCAPresRules(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap)
     : XCAccess(xerces_scope, xsd_dir, xsdmap, prepare_info_presrules())
     { }
};

XCAccessMgr::XCAccessMgr(std::string const& locale, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap
	, std::string const& default_domain, boost::shared_ptr<Storage> storage)
	 : xerces_scope(xml::XercesScope::create(locale))
{
    XCACapabilities* xcaps(new XCACapabilities(xerces_scope, xsd_dir, xsdmap));

    //TODO: Options::disabled services

    //NOTE: extenzije aplikacija se uzimaju prema zadanim opcijama zasebno u svakom
    //od izvedenih tipova od XCAccess

    //TODO: u openxcap imaju i aplikacije dialog-rules i watchers
    apps.push_back(boost::shared_ptr<XCAccess>(xcaps));
    apps.push_back(boost::shared_ptr<XCAccess>(new XCAResourceLists(xerces_scope, xsd_dir, xsdmap)));
    apps.push_back(boost::shared_ptr<XCAccess>(new XCARLSServices(xerces_scope, xsd_dir, xsdmap, default_domain)));
    apps.push_back(boost::shared_ptr<XCAccess>(new XCAPIDFManipulation(xerces_scope, xsd_dir, xsdmap)));
    apps.push_back(boost::shared_ptr<XCAccess>(new XCAPresRules(xerces_scope, xsd_dir, xsdmap)));
//    apps.push_back(boost::shared_ptr<XCAccess>(new XCADirectory(xerces_scope, xsd_dir, xsdmap)));
//    apps.push_back(boost::shared_ptr<XCAccess>(new XCAOMAPresRules(xerces_scope, xsd_dir, xsdmap)));
//    apps.push_back(boost::shared_ptr<XCAccess>(new XCAOMAXcapDirectory(xerces_scope, xsd_dir, xsdmap)));
//    apps.push_back(boost::shared_ptr<XCAccess>(new XCAOMAPresContent(xerces_scope, xsd_dir, xsdmap)));
//    apps.push_back(boost::shared_ptr<XCAccess>(new XCAOMAConvHistory(xerces_scope, xsd_dir, xsdmap)));
//    apps.push_back(boost::shared_ptr<XCAccess>(new XCAOMADeferredList(xerces_scope, xsd_dir, xsdmap)));
//    apps.push_back(boost::shared_ptr<XCAccess>(new XCAOMAGroupUsageList(xerces_scope, xsd_dir, xsdmap)));

    xcaps->initialize_caps(apps);

    for(size_t i(0); i<apps.size(); ++i) {
        apps[i]->set(storage);
    }
}

boost::shared_ptr<XCAccess> XCAccessMgr::find(std::string const& auid) const
{
    for(size_t i(0); i<apps.size(); ++i) {
        if(apps[i]->info().id == auid) return apps[i];
    }

    return boost::shared_ptr<XCAccess>();
}

}
}
