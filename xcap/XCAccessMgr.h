#include <boost/shared_ptr.hpp>
#include <vector>
#include <map>
#include <scarlet/xml/xmldefs.h>
namespace scarlet {
namespace xml {
class XercesScope;
typedef boost::shared_ptr<XercesScope> XercesScopePtr;
}
namespace xcap {

class XCAccess;
struct Storage;


// u jednoj niti dovoljna je samo po jedna instanca svake od aplikacija jer se
// u jednom intervalu obrade zahtjeva (_Task) u jednoj niti koristi se samo jedna od aplikacija
class XCAccessMgr {
	xml::XercesScopePtr xerces_scope;
	std::vector<boost::shared_ptr<XCAccess> > apps;

    explicit XCAccessMgr(void); //NE

public:

    boost::shared_ptr<XCAccess> find(std::string const& auid) const;

    XCAccessMgr(std::string const& locale, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap, std::string const& default_domain, boost::shared_ptr<Storage> storage);
};

}
}
