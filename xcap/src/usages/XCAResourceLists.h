#include "scarlet/xcap/XCAccess.h"
#include "scarlet/xcap/ElementChecker.h"
#include <list>

namespace scarlet {
namespace xcap {

class URIParser;

class ResourceListsChecker  {

    boost::shared_ptr<xml::match_prefixed_name> const is_list;
    boost::shared_ptr<xml::match_prefixed_name> const is_entry;
    boost::shared_ptr<xml::match_prefixed_name> const is_entry_ref;
    boost::shared_ptr<xml::match_prefixed_name> const is_external;

protected:
    ResourceListsChecker(void);

    virtual ~ResourceListsChecker();

    XCAPErrorPtr check_list(xml::XMLSelect const& selection, xcapuri_t const& uri) const;

    virtual void extension_list_checks(std::vector<ElementCheckerPtr>& exchecks) const { }
public:
    static u8vector_t const resource_lists_auid;
    static u8vector_t const resource_lists_namespace;
};


class XCAResourceLists : public XCAccess, protected ResourceListsChecker {
	XCAResourceLists(void) = delete;
	response_code_e constraints(
        u8vector_t& docresponse
        , doctree_ptr const& xr_doc
        , xcapuri_t const& uri
        , u8vector_t const& olddoc
    ) const;

public:
    XCAResourceLists(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap);

    void extension_list_checks(std::vector<ElementCheckerPtr>& exchecks) const;

    app_usage_t prepare_info(void);
};


class RLSServicesChecker : public ResourceListsChecker {
	RLSServicesChecker(void) = delete;

	boost::shared_ptr<xml::match_prefixed_name> const is_resource_list;
    boost::shared_ptr<xml::match_prefixed_name> const is_list_svc;
    boost::shared_ptr<xml::match_prefixed_name> const is_service;
    boost::shared_ptr<URIParser> uriparser;

    XCAPErrorPtr check_service(xml::XMLSelect const& selection, xcapuri_t const& uri) const;

protected:
    RLSServicesChecker(std::string const& default_domain);

    virtual ~RLSServicesChecker();

    XCAPErrorPtr check_rls(xml::XMLSelect const& selection, xcapuri_t const& rquri) const;

    virtual void extension_service_checks(std::vector<ElementCheckerPtr>& exchecks) const { }
public:
    static u8vector_t const rls_services_namespace;
};


class XCARLSServices : public XCAccess, protected RLSServicesChecker {
	XCARLSServices(void) = delete;

	response_code_e constraints(
        u8vector_t& docresponse
        , doctree_ptr const& xr_doc
        , xcapuri_t const& uri
        , u8vector_t const& olddoc
    ) const;

public:
    //TODO: globalni 'index' se dinamicki pravi, nije u storage
	XCARLSServices(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap, std::string const& default_domain);

    void extension_list_checks(std::vector<ElementCheckerPtr>& exchecks) const;
    void extension_service_checks(std::vector<ElementCheckerPtr>& exchecks) const;

    app_usage_t prepare_info(void);
};

}
}
