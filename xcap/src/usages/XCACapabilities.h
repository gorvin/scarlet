#include "scarlet/xcap/XCAccess.h"

namespace scarlet {
namespace xcap {

/** xcap-caps, formira XML dokument sa podrzanim mogucnostima (aplikacijama).
Ne moze PUT/DELETE, samo GET i vazi samo za globalno podstablo
*/
class XCACapabilities : public XCAccess {
	XCACapabilities(void) = delete;

    std::string const xcap_caps;

    int getdoc(
        u8vector_t& doc
        , std::string& etag
        , xcapuri_t const& rquri
        , std::string const& domain
    ) const;

    int putdoc(
        xcapuri_t const& /*rquri*/
        , rawcontent_t const& /*doc*/
        , std::string const& /*etagnew*/
        , std::string const& /*etagprev*/
        , std::string const& /*domain*/
    ) const
    {
        return -1;
    }

    int deldoc(
        xcapuri_t const& /*rquri*/
        , std::string const& /*etagprev*/
        , std::string const& /*domain*/
    ) const
    {
        return -1;
    }

public:

    void initialize_caps(std::vector<boost::shared_ptr<XCAccess> > const& allapps);

    XCACapabilities(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap);
};

}
}
