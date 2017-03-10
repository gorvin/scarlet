#include "XCACapabilities.h"
#include "bmu/Logger.h"

namespace scarlet {
namespace xcap {

app_usage_t prepare_info_caps(void)
{
    std::string const xcap_caps_extensions[] = { "caps-test-ext1", "caps-test-ext2" };//probno

    app_usage_t tmp = {
        "xcap-caps",
        "application/xcap-caps+xml",
        "urn:ietf:params:xml:ns:xcap-caps",
        vstrings_t(),
        "index",
        true,
        vstrings_t(STRINGS_BEG_END(xcap_caps_extensions))
    };

    return tmp;
}

/*<?xml version="1.0" encoding="UTF-8"?>
<xcap-caps xmlns="urn:ietf:params:xml:ns:xcap-caps"
        xmlns:xsi="htt//www.w3.org/2001/XMLSchema-instance"
        xsi:schemaLocation="urn:ietf:params:xml:ns:xcap-caps xcap-caps.xsd ">
    <auids>
        <auid>usage_auid</auid>
        ...
   </auids>
  <extensions>
        <extension>usage_auid</extension>
        ...
  </extensions>
  <namespaces>
      <namespace>urn:ietf:params:xml:ns:usage_ns</namespace>
      ...
  </namespaces>
</xcap-caps>*/
XCACapabilities::XCACapabilities(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap)
 : XCAccess(xerces_scope, xsd_dir, xsdmap, prepare_info_caps())
{ }


void XCACapabilities::initialize_caps(std::vector<boost::shared_ptr<XCAccess> > const& allapps)
{
    std::string auids;
    std::string nspaces;
    std::string extensions;

    for(size_t i=0; i<allapps.size(); ++i) {
        app_usage_t const& info = allapps[i]->info();
        auids.append(std::string("        <auid>") + info.id + "</auid>\n");
        nspaces.append(std::string("        <namespace>") + info.ns + "</namespace>\n");
        for(size_t j=0; j!=info.extensions.size(); ++j) {
            std::string const& ext_id(info.extensions[j]);
            //da bi se znalo za koju aplikaciju je ekstenzija stavljam prefix 'auid:'
            extensions.append(std::string("        <extension>") + info.id + ":" + ext_id + "</extension>\n");
            //TODO: izbaci ovo pa iza ove petlje dodaj drugu koj popunjava namespaces iz info.ns_other
            //NOTE: ns_other su sustinski prave ekstenzije a ovi identifikatori 'ext_id' su vise
            //informativnog karaktera, jer za jednu ekstenziju moze biti vise dodatnih namespaceova
            //a dodavanjem nekog namespacea mora se u opcijama mapirati i njegov namespace u xml
            //schemu koja onda dozvoljava nove elemente u dokumentu
            nspaces.append(std::string("        <namespace>") + info.ns + ":" + ext_id + "</namespace>\n");
        }
    }

    nspaces.append("        <namespace>urn:ietf:params:xml:ns:xcap-error</namespace>\n");

    (std::string("<?xml version='1.0' encoding='UTF-8'?>\n"
                "<xcap-caps xmlns='urn:ietf:params:xml:ns:xcap-caps'>\n")
              + "    <auids>\n" + auids + "    </auids>\n"
              + "    <extensions>\n" + extensions + "    </extensions>\n"
              + "    <namespaces>\n" + nspaces + "    </namespaces>\n"
              + "</xcap-caps>\n"
    ).swap(const_cast<std::string&>(xcap_caps));
}


int XCACapabilities::getdoc(
    u8vector_t& doc
    , std::string& etag
    , xcapuri_t const& rquri
    , std::string const& /*domain*/
) const
{
    if(rquri.docpath.context != (u8unit_t const*)"global") {
        DBGMSGAT("Failed - capabilites document only available globaly");
        doc.clear();
        etag.clear();
        return 0;
    }

    if(rquri.docpath.docname != (u8unit_t const*)"index") {
        DBGMSGAT("Failed - document name is not 'index'");
        doc.clear();
        etag.clear();
        return 0;
    }

    DBGMSGAT("Found global cababilities document named 'index'");

    doc.assign(xcap_caps.begin(), xcap_caps.end());
    etag = "xcap-caps-dummy-etag";

    return 0;
}

}
}
