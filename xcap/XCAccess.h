#ifndef XCACCESS_H
#define XCACCESS_H

#include "scarlet/xcap/XMLEngine.h"
#include "scarlet/xcap/xcadefs.h"

namespace scarlet {
namespace xcap {

typedef std::vector<std::string> vstrings_t;

#define STRINGS_BEG_END(x) &x[0], &x[0] + NSTRINGS(x)
#define NSTRINGS(x) ( sizeof(x)/sizeof(std::string) )

struct app_usage_t {
    std::string id; /**< Ime XML seme bez ekstenzije .xsd (e.g. 'xcap-caps' or 'rls-services' or 'resource-lists' or ...)*/
    std::string mime; /**< The default MIME-type of the document associated to this auid. */
    std::string ns; /**< The default namespace associated to this auid. */
    vstrings_t  ns_other; /**< Other namesaces used with this auid, important for maping namespaces to *.xsd */
    std::string docname; /**< The default name of the document associated to this auid. */
    bool        scope; /**< Indicates the auid scope, global or not. */
    vstrings_t  extensions; /**< [infomational] List of supported extensions, examples for pidf-manipulation: (RPID) RFC 4480, (CIPID) 4482*/
    static std::string const w3namespace;
};

//enum xcap_applications_e {
//    APP_IETF_XCAP_CAPS = 0,//RFC 4825
//    APP_IETF_RESOURCE_LISTS,//RFC 4826 3.4.1
//    APP_IETF_RLS_SERVICES,//RFC 4826 4.4.1
//    APP_IETF_PIDF_MANIPULATION,//RFC 4827
//    APP_IETF_PRES_RULES,//RFC 5025 9.1 + document format RFC 4745
//    APP_IETF_DIRECTORY,//users draft-garcia-simple-xcap-directory-00 section 9.1
//    APP_OMA_PRES_RULES,//OMA-TS-Presence_SIMPLE_XDM-V1_1-20080627-A section 5.1.1.22
//    APP_OMA_XCAP_DIRECTORY,//OMA-TS-XDM_Core-V1_1-20080627-A section 6.7.2.1
//    APP_OMA_PRES_CONTENT,//OMA-TS-Presence-SIMPLE_Content_XDM-V1_0-20081223-C section 5.1.2
//    APP_OMA_CONV_HISTORY,//OMA-TS-IM_XDM-V1_0-20070816-C section 5.1.2
//    APP_OMA_DEFERRED_LIST,//OMA-TS-IM_XDM-V1_0-20070816-C section 5.2.2
//    APP_OMA_GROUP_USAGE_LIST,//OMA-TS-XDM_Shared-V1_1-20080627-A subclause 5.2.2
//    APP_ENUM_SIZE
//};


//class ApplicationsDefs {
//    std::map<std::string, xcap_applications_e> indexes;
//    std::vector<app_usage_t>                   infos;
//public:
//    app_usage_t const& info(xcap_applications_e id) const { return infos[id]; }
//    xcap_applications_e index(std::string const& auid) const;
//    ApplicationsDefs(void);
//};
//

struct Storage;

struct xcacontext_t {
	virtual ~xcacontext_t(void) { }
	//request handling part
	virtual std::string const& rqUsername(void) const = 0;
	virtual std::string const& rqDomain(void) const = 0;
	virtual xcapuri_t const& rqUri(void) const = 0;
	virtual std::vector<std::string> const& rqIfEtags(void) const = 0;
	virtual std::vector<std::string> const& rqIfNoEtags(void) const = 0;
	enum method_e {
		RQ_METHOD_GET,
		RQ_METHOD_PUT,
		RQ_METHOD_DELETE
		//TODO: RQ_METHOD_POST //OMA XDM V2.1, XDCP (XDM Command Protocol) document
		//TODO: RQ_SUBSCRIBE //OMA XDM V2.1, SIP/IP Core
	};
	virtual method_e rqMethod(void) const = 0;
	virtual rawcontent_t const& rqBody(void) const = 0;
	virtual std::string const& rqMime(void) const = 0;
	//response handling part
	virtual void reCode(response_code_e const&) = 0;
	virtual u8vector_t& reBodyOut(void) = 0;
	virtual std::string& reEtagOut(void) = 0;
	virtual void reMime(std::string const& mime) { reMimeOut() = mime; }
	virtual std::string& reMimeOut() = 0;
	virtual std::map<std::string, std::string>& reExtraHeadersOut(void) = 0;
};

class XCAccess : protected XMLMethods {

    //xcap_applications_e const _type;
    app_usage_t  app_info;
    boost::shared_ptr<Storage> storage;

    explicit XCAccess(void); //NE

    //static ApplicationsDefs const allinfos;

    static std::string make_map_ns_to_xsd(std::map<std::string, std::string> const& xsdmap, app_usage_t const& info);

    static bool perform_method(
        std::vector<std::string> const& ifetags
        , std::vector<std::string> const& ifnoetags
        , std::string const& etag
    );

protected:
	//xsdmap je Options::instance().namespace_schema()
    XCAccess(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap, app_usage_t const& app_info);

    virtual int getdoc(
        u8vector_t& doc
        , std::string& etag
        , xcapuri_t const& rquri
        , std::string const& domain
    ) const;

    //etagprev je empty za insert
    virtual int putdoc(
        xcapuri_t const& rquri
        , rawcontent_t const& doc
        , std::string const& etagnew
        , std::string const& etagprev
        , std::string const& domain
    ) const;

    virtual int deldoc(
        xcapuri_t const& rquri
        , std::string const& etagprev
        , std::string const& domain
    ) const;

    enum privileges_e {
        READ_ACCESS,
        WRITE_ACCESS
    };

    u8vector_t xml_invalid_error(void) const;

    virtual bool authorized(
        std::string const& username
        , u8vector_t const& xui
        , privileges_e access
    ) const;

    virtual response_code_e constraints(
        u8vector_t& docresponse
        , doctree_ptr const& xr_doc
        , xcapuri_t const& rquri
        , u8vector_t const& olddoc //daje sansu za provjeru ima li nedozvoljenih promjena elemenata
    ) const;

	response_code_e get(xcacontext_t& ctx) const;
	response_code_e put(xcacontext_t& ctx) const;
	response_code_e del(xcacontext_t& ctx) const;

public:

    void handle_request(xcacontext_t& ctx) const;

    //static ApplicationsDefs const& definitions(void) { return allinfos; }
    app_usage_t const& info(void) const { return app_info; }

    //static Storage& storage(void);
    void set(boost::shared_ptr<Storage> strg);

    virtual ~XCAccess() { }
};

}
}
#endif //XCACCESS_H
