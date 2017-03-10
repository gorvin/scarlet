#include "XCAResourceLists.h"
#include "scarlet/xcap/URIParser.h"
#include "bmu/GenericURI.h"
#include "bmu/Logger.h"

namespace scarlet {
namespace xcap {

u8vector_t const ResourceListsChecker::resource_lists_auid((u8unit_t const*)"resource-lists");
u8vector_t const ResourceListsChecker::resource_lists_namespace((u8unit_t const*)"urn:ietf:params:xml:ns:resource-lists");
u8vector_t const RLSServicesChecker::rls_services_namespace((u8unit_t const*)"urn:ietf:params:xml:ns:rls-services");

ResourceListsChecker::ResourceListsChecker(void)
 : is_list(new xml::match_prefixed_name(u8vector_t((u8unit_t*)"list"), resource_lists_namespace))
 , is_entry(new xml::match_prefixed_name(u8vector_t((u8unit_t*)"entry"), resource_lists_namespace))
 , is_entry_ref(new xml::match_prefixed_name(u8vector_t((u8unit_t*)"entry-ref"), resource_lists_namespace))
 , is_external(new xml::match_prefixed_name(u8vector_t((u8unit_t*)"external"), resource_lists_namespace))
 { }

ResourceListsChecker::~ResourceListsChecker() { }

RLSServicesChecker::RLSServicesChecker(std::string const& default_domain)
 : is_resource_list(new xml::match_prefixed_name(u8vector_t((u8unit_t*)"resource-list"), rls_services_namespace))
 , is_list_svc(new xml::match_prefixed_name(u8vector_t((u8unit_t*)"list"), rls_services_namespace))//lista u RLS namespaceu
 , is_service(new xml::match_prefixed_name(u8vector_t((u8unit_t*)"service"), rls_services_namespace))
 , uriparser(new URIParser(default_domain))
 { }

RLSServicesChecker::~RLSServicesChecker() { }

XCAPErrorPtr check_reference_absolute(ElementChecker const* checker, u8vector_t const& value, xcapuri_t const&)
{
    bmu::GenericURI uri(std::string(value.begin(), value.end()));
    DBGMSGAT("Parsed URI as scheme='" << uri.scheme()
             << "' host='" << uri.host() << "' path='" << uri.path() << "'");
    //valjan absolute URI je ako ima http uri scheme i nema #fragment
    if(!uri.is_absolute() || uri.path().find('#') != std::string::npos) { //fragment
        return checker->fail_constraint("value is not absolute URI");
    }
    if(uri.scheme() != "http" && uri.scheme() != "https") {
        return checker->fail_constraint("value is absolute URI but with unsupported scheme");
    }
    return ElementChecker::ok();
}

XCAPErrorPtr check_reference_relative_path(ElementChecker const* checker, u8vector_t const& value, xcapuri_t const&)
{
    bmu::GenericURI uri(std::string(value.begin(), value.end()));

    DBGMSGAT("Parsed URI as scheme='" << uri.scheme()
             << "' host='" << uri.host() << "' path='" << uri.path() << "'");

    //valjan relative-path je ako nema uri scheme i ne pocinje sa '/'
    //if(!uri.scheme.empty() || uri.path.empty() || uri.path[0] == '/') {
    if(uri.is_absolute()/* || uri.path().empty()*/) {
        DBGMSGAT("URL is not relative-path reference: " << bmu::utf8_string(value));
        return checker->fail_constraint("value is not relative-path reference");
    }

    return ElementChecker::ok();
}

XCAPErrorPtr check_auid_and_xui(
    boost::shared_ptr<URIParser> uriparser
	, std::string const& default_domain
    , ElementChecker const* checker
    , u8vector_t const& value
    , xcapuri_t const& rquri
)
{
    xcapuri_t xuri;
    u8vector_t domain;
    uriparser->reset(xuri.docpath, xuri.npath, domain, bmu::utf8_string(value));

    if(xuri.docpath.auid != ResourceListsChecker::resource_lists_auid || xuri.docpath.xui != rquri.docpath.xui) {
        return checker->fail_constraint(
            "value of child text node is absolute URI but AUID or XUI is not same as in request"
        );
    }

    return ElementChecker::ok();
}

//NOTE: The server is not responsible for verifying that the <entry> reference resolves to an
//      <entry> element in a document within the same XCAP root
//NOTE: Klijent mora biti siguran da je <entry> refenca u istom XCAP rootu. Referenciranje
//      u dokument drugog korisnika nece funkcionisati jer kasnije nece moci dobaviti
//      referencirani dio is tudjeg dokumenta jer nema na to pravo, dakle server
//      se ne mora brinuti da li je referenca u tudji dokument.
//NOTE: The server is not responsible for verifying that the <entry-ref> anchor URI resolves to a
//      <list> element in a document
XCAPErrorPtr ResourceListsChecker::check_list(xml::XMLSelect const& selection, xcapuri_t const& rquri) const
{
    std::vector<ElementCheckerPtr> allchecks;

    {
        ElementCheckerPtr rl_list(new ElementChecker(
			is_list
			, boost::bind(&ResourceListsChecker::check_list, this, boost::placeholders::_1, boost::placeholders::_2)
			));
        rl_list->push(UniqueValCheck(MakaAttributeExtractor((u8unit_t*)"name")));
        allchecks.push_back(rl_list);
    }

    {
        ElementCheckerPtr rl_entry(new ElementChecker(is_entry));
        rl_entry->push(UniqueValCheck(MakaAttributeExtractor((u8unit_t*)("uri"))));
        allchecks.push_back(rl_entry);
    }

    {
        ElementCheckerPtr rl_entry_ref(new ElementChecker(is_entry_ref));
		UniqueValCheck refcheck(MakaAttributeExtractor((u8unit_t*)("ref")));
        refcheck.push(check_reference_relative_path);

        rl_entry_ref->push(refcheck);
        allchecks.push_back(rl_entry_ref);
    }

    {
        ElementCheckerPtr rl_external(new ElementChecker(is_external));
		UniqueValCheck anchorcheck(MakaAttributeExtractor((u8unit_t*)("anchor")));
        anchorcheck.push(check_reference_absolute);

        rl_external->push(anchorcheck);
        allchecks.push_back(rl_external);
    }

    std::vector<ElementCheckerPtr> exchecks;
    extension_list_checks(exchecks);
    allchecks.insert(allchecks.end(), exchecks.begin(), exchecks.end());

	xml::XMLSelect rall(selection, u8vector_t(), u8vector_t(), xml::nsbindings_t(), resource_lists_namespace);//all children

    for(size_t i=0; i<rall.count(); ++i) {
        xmlnode_t* node(rall[i]);

        for(size_t j=0; j<allchecks.size(); ++j) {
            ElementCheckerPtr tocheck(allchecks[j]);
            if(tocheck->match(node)) {
                XCAPErrorPtr error(tocheck->check(node, rquri));
                if(error) return error;
                break;//this element is checked go to next element
            }
        }

        //NOTE: unknown extension node must be accepted as is
    }

    return ElementChecker::ok();
}

XCAPErrorPtr RLSServicesChecker::check_service(xml::XMLSelect const& selection, xcapuri_t const& rquri) const
{
    std::vector<ElementCheckerPtr> allchecks;

    {
        ElementCheckerPtr rls_list(new ElementChecker(
			is_list_svc
			, boost::bind(&RLSServicesChecker::check_list, this, boost::placeholders::_1, boost::placeholders::_2)
			));
        rls_list->push(UniqueValCheck(MakaAttributeExtractor((u8unit_t*)("name"))));
        allchecks.push_back(rls_list);
    }

    {
        ElementCheckerPtr rls_resource_list(new ElementChecker(is_resource_list));
        //URI je izmedju tagova <resource-list> ... </resource-list>, nije atribut
        UniqueValCheck subcheck(ChildValueExtractor);
        subcheck.push(check_reference_absolute);
		auto tmp = boost::bind(check_auid_and_xui, uriparser, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4);
        //subcheck->push();

        //rls_resource_list->push(boost::shared_ptr<PartialCheckBase>(subcheck));
        rls_resource_list->push(subcheck);
        allchecks.push_back(rls_resource_list);
    }

    std::vector<ElementCheckerPtr> exchecks;
    extension_service_checks(exchecks);
    allchecks.insert(allchecks.end(), exchecks.begin(), exchecks.end());

	xml::XMLSelect rall(selection, u8vector_t(), u8vector_t(), xml::nsbindings_t(), rls_services_namespace);//all children

    for(size_t i=0; i<rall.count(); ++i) {
        xmlnode_t* node(rall[i]);

        for(size_t j=0; j<allchecks.size(); ++j) {
            ElementCheckerPtr tocheck(allchecks[j]);
            if(tocheck->match(node)) {
                XCAPErrorPtr error(tocheck->check(node, rquri));
                if(error) return error;
                break;//this element is checked go to next element
            }
        }

        //NOTE: unknown extension node must be accepted as is
    }

    return ElementChecker::ok();
}

XCAPErrorPtr RLSServicesChecker::check_rls(xml::XMLSelect const& selection, xcapuri_t const& rquri) const
{
    std::vector<ElementCheckerPtr> allchecks;

    {
        ElementCheckerPtr rls_service(new ElementChecker(
			is_service
			, boost::bind(&RLSServicesChecker::check_service, this, boost::placeholders::_1, boost::placeholders::_2)
			));

        //TODO: dodaj provjeru ima li u globalnom rls-services index dokumentu
        //  mogao bi jednostavno XCAccess::get( ... services[@uri=attvalue]...)

        rls_service->push(UniqueValCheck(MakaAttributeExtractor((u8unit_t*)("uri"))));
        allchecks.push_back(rls_service);
    }

	xml::XMLSelect rall(selection, u8vector_t(), u8vector_t(), xml::nsbindings_t(), rls_services_namespace);//all children

    for(size_t i=0; i<rall.count(); ++i) {
        xmlnode_t* node(rall[i]);

        for(size_t j=0; j<allchecks.size(); ++j) {
            ElementCheckerPtr tocheck(allchecks[j]);
            if(tocheck->match(node)) {
                XCAPErrorPtr error(tocheck->check(node, rquri));
                if(error) return error;
                break;//this element is checked go to next element
            }
        }

        //trebalo bi da su moguci samo <service> elementi ispod root, nije spomenuta prosirivost u RFC
    }

    return ElementChecker::ok();
}

XCAResourceLists::XCAResourceLists(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap)
 : XCAccess(xerces_scope, xsd_dir, xsdmap, prepare_info())
 , ResourceListsChecker()
 { }

response_code_e XCAResourceLists::constraints(
    u8vector_t& docresponse
    , doctree_ptr const& xr_doc
    , xcapuri_t const& rquri
    , u8vector_t const& /*olddoc*/
) const
{
    //NOTE: za resource-lists se ne zahtijeva koje ce biti ime dokumenta
    //vjerovatno zato jer nema ni resource interdependencies
    XCAPErrorPtr error(ResourceListsChecker::check_list(xml::XMLSelect(xr_doc), rquri));
    if(error) {
        error->write(docresponse, (u8unit_t const*)"resource-lists");
        DBGMSGAT(bmu::utf8_string(docresponse));
        return XCAP_FAIL_CONSTRAINTS;
    }

    docresponse.clear();
    return XCAP_OK;
}

void XCAResourceLists::extension_list_checks(std::vector<ElementCheckerPtr>& exchecks) const
{
    //TODO: ovdje dodajes provjere elemenata iz prosirenja zadanih u opcijama
}

app_usage_t XCAResourceLists::prepare_info(void)
{
    app_usage_t tmp = {
		bmu::utf8_string(ResourceListsChecker::resource_lists_auid),
        "application/resource-lists+xml",
		bmu::utf8_string(ResourceListsChecker::resource_lists_namespace),
        vstrings_t(), //TODO: ovdje treba da dodajes i namespaceje prosirenja zadanih u opcijama
        "index",
        false,
        vstrings_t()
    };
    const_cast<u8vector_t*>(&XCAResourceLists::resource_lists_auid)->assign(tmp.id.begin(), tmp.id.end());
    const_cast<u8vector_t*>(&XCAResourceLists::resource_lists_namespace)->assign(tmp.ns.begin(), tmp.ns.end());
    return tmp;
}

XCARLSServices::XCARLSServices(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap, std::string const& default_domain)
 : XCAccess(xerces_scope, xsd_dir, xsdmap, prepare_info())
 , RLSServicesChecker(default_domain)
 { }

response_code_e XCARLSServices::constraints(
    u8vector_t& docresponse
    , doctree_ptr const& xr_doc
    , xcapuri_t const& rquri
    , u8vector_t const& /*olddoc*/
) const
{
    //TODO: provjera da li je u rquri ime dokumenta 'index'
    //Zahtjev je za GLOBALNI da se naziva 'index' a za users se preporucuje isto ime jer inace
    //nece biti obuhvacen unutar globalnog dokumenta koji se dinamicki pravi obuhvatajuci sve
    //korisnicke dokumente sa imenom 'index'
    XCAPErrorPtr error(RLSServicesChecker::check_rls(xml::XMLSelect(xr_doc), rquri));
    if(error) {
        error->write(docresponse, (u8unit_t const*)"rls-services");
        DBGMSGAT(bmu::utf8_string(docresponse));
        return XCAP_FAIL_CONSTRAINTS;
    }

    docresponse.clear();
    return XCAP_OK;
}


void XCARLSServices::extension_list_checks(std::vector<ElementCheckerPtr>& exchecks) const
{
    //TODO: ovdje dodajes provjere elemenata iz prosirenja zadanih u opcijama
}

void XCARLSServices::extension_service_checks(std::vector<ElementCheckerPtr>& exchecks) const
{
    //TODO: ovdje dodajes provjere elemenata iz prosirenja zadanih u opcijama
}

app_usage_t XCARLSServices::prepare_info(void)
{
    std::string const rls_ns_other[] = { "urn:ietf:params:xml:ns:resource-lists", };

    app_usage_t tmp = {
        "rls-services",
        "application/rls-services+xml",
		bmu::utf8_string(RLSServicesChecker::rls_services_namespace),
        vstrings_t(STRINGS_BEG_END(rls_ns_other)), //TODO: ovdje treba da dodajes i namespaceje prosirenja zadanih u opcijama
        "index",
        false,
        vstrings_t()
    };
    const_cast<u8vector_t*>(&XCARLSServices::rls_services_namespace)->assign(tmp.ns.begin(), tmp.ns.end());
    return tmp;
}

}
}
