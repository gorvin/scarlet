#include "scarlet/xcap/XCAccess.h"
#include "scarlet/xcap/Storage.h"
//#include "scarlet/xcap/Options.h"
#include "bmu/Logger.h"
#include "bmu/exdefs.h"
#include <boost/uuid/uuid.hpp> //boost version >= 1.42
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace scarlet {
namespace xcap {

std::string const app_usage_t::w3namespace("http://www.w3.org/XML/1998/namespace");

//    xmlstring schemaLocations(transcoded<XMLCh>(
//      "http://example.com                     test_schema.xsd       "
//      "http://www.w3.org/XML/1998/namespace   xml.xsd               "
//      "urn:ietf:params:xml:ns:xcap-error      xcap-error.xsd        "
//      "urn:ietf:params:xml:ns:xcap-caps       xcap-caps.xsd         "
//      "urn:oma:xml:xdm:xcap-directory         xcap-directory.xsd    "
//      "urn:ietf:params:xml:ns:resource-lists  resource-lists.xsd    "
//      "urn:ietf:params:xml:ns:rls-services    rls-services.xsd      "
//      "urn:ietf:params:xml:ns:common-policy   common-policy.xsd     "
//      "urn:ietf:params:xml:ns:pres-rules      presence-rules.xsd    "
//      "urn:ietf:params:xml:ns:pidf            pidf.xsd              "
//      "urn:ietf:params:xml:ns:pidf:data-model pidf-data-model.xsd   "
//      "http://openxcap.org/ns/watchers        openxcap-watchers.xsd ")
//    );
std::string XCAccess::make_map_ns_to_xsd(std::map<std::string, std::string> const& xsdmap, app_usage_t const& info)
{
    static std::string const white(" ");
	try {
		BOOST_ASSERT(xsdmap.find(app_usage_t::w3namespace) != xsdmap.end());
		BOOST_ASSERT(xsdmap.find(info.ns) != xsdmap.end());
		std::string nsxsd(app_usage_t::w3namespace + white + xsdmap.at(app_usage_t::w3namespace) + white);
		nsxsd.append(info.ns + white + xsdmap.at(info.ns) + white);

		for (size_t i = 0; i<info.ns_other.size(); ++i) {
			BOOST_ASSERT(xsdmap.find(info.ns_other[i]) != xsdmap.end());
			nsxsd.append(info.ns_other[i] + white + xsdmap.at(info.ns_other[i]) + white);
		}
		return nsxsd;
	}
	catch (std::out_of_range const& e) {
		std::wclog << "FATAL: in function " << __FUNCTION__  << ": " << boost::diagnostic_information(e) << std::endl;
		std::terminate();//(FATAL)
	}
	return std::string();
}

XCAccess::XCAccess(xml::XercesScopePtr xerces_scope, std::string const& xsd_dir, std::map<std::string, std::string> const& xsdmap, app_usage_t const& app_info)
 : XMLMethods(xerces_scope
           , bmu::utf8_vector(app_info.ns)
           , make_map_ns_to_xsd(xsdmap, app_info)
           , xsd_dir) // boost::filesystem::path(Options::instance().topdir()) / Options::instance().subdir_xsd()
 , app_info(app_info)
 { }

bool XCAccess::authorized(
    std::string const& username
    , u8vector_t const& xui
    , privileges_e access
) const
{
    if(xui.empty()) return access == READ_ACCESS;

    DBGMSGAT("Username '" << username << "', XUI '" << bmu::utf8_string(xui) << "'");

    return xui.size() == username.size()
        && xui.compare(0, username.size(), reinterpret_cast<u8unit_t const*>(username.data())) == 0;
}

response_code_e XCAccess::constraints(
    u8vector_t& /*docresponse*/
    , doctree_ptr const& /*xr_doc*/
    , xcapuri_t const& /*uri*/
    , u8vector_t const& /*olddoc*/
) const
{
    return XCAP_OK;
}

void XCAccess::set(boost::shared_ptr<Storage> newstorage)
{
    storage = newstorage;
}

bool XCAccess::perform_method(
    std::vector<std::string> const& ifetags
    , std::vector<std::string> const& ifnoetags
    , std::string const& etag
)
{
    if(!ifetags.empty()) {
        if(std::find(ifetags.begin(), ifetags.end(), "*") != ifetags.end())
            return !etag.empty();

        return std::find(ifetags.begin(), ifetags.end(), etag) == ifetags.end();

    } else if(!ifnoetags.empty()) {
        if(std::find(ifnoetags.begin(), ifnoetags.end(), "*") != ifnoetags.end())
            return etag.empty();

        return std::find(ifnoetags.begin(), ifnoetags.end(), etag) == ifnoetags.end();
    }
    return true;
}

int XCAccess::getdoc(
    u8vector_t& doc
    , std::string& etag
    , xcapuri_t const& rquri
    , std::string const& domain
) const
{
    return storage->get(doc, etag, rquri.docpath, domain);
}

int XCAccess::putdoc(
    xcapuri_t const& rquri
    , rawcontent_t const& docpart
    , std::string const& etagnew
    , std::string const& etagprev
    , std::string const& domain
) const
{
    return storage->put(rquri.docpath, docpart, etagnew, etagprev, domain);
}

int XCAccess::deldoc(
    xcapuri_t const& rquri
    , std::string const& etagprev
    , std::string const& domain
) const
{
    return storage->del(rquri.docpath, etagprev, domain);
}

response_code_e XCAccess::get(xcacontext_t& ctx) const
{
	ctx.reBodyOut().clear();
	ctx.reEtagOut().clear();
	ctx.reMimeOut().clear();

    if(!authorized(ctx.rqUsername(), ctx.rqUri().docpath.xui, READ_ACCESS))
        return XCAP_FAIL_AUTHORIZATION; //Not Found;

    u8vector_t doc;
    if(getdoc(doc, ctx.reEtagOut(), ctx.rqUri(), ctx.rqDomain()) != 0)
        return XCAP_ERROR_INTERNAL;

    assert((!doc.empty() && !ctx.reEtagOut().empty()) || (doc.empty() && ctx.reEtagOut().empty()));

    if(doc.empty())
        return XCAP_FAIL_NOT_FOUND;

    if(!perform_method(ctx.rqIfEtags(), ctx.rqIfNoEtags(), ctx.reEtagOut()))
        return XCAP_FAIL_IF_PERFORM;

    if(ctx.rqUri().npath.empty()) {
		ctx.reBodyOut().swap(doc);//etag je vec spreman
		ctx.reMime(app_info.mime);
        return XCAP_OK;
    }

    return get_xpath(ctx.reBodyOut(), ctx.reMimeOut(), doc, ctx.rqUri().npath, ctx.rqUri().prefixes);
}

u8vector_t XCAccess::xml_invalid_error(void) const
{
    std::string elstart;

    switch(XMLEngine::document_validity()) {
    case xml::XML_NOT_SCHEMA_VALID: {
        elstart = "<schema-validation-error"; // />
        break;
    } case xml::XML_NOT_WELL_FORMED : {
        elstart = "<not-well-formed";
        break;
    } case xml::XML_NOT_UTF8: {
        elstart = "<not-utf8";
        break;
    } default:
        assert(0);
    }

    std::string msg;
    std::vector<std::string> const& xml_msg(XMLEngine::document_errors());

    if(!xml_msg.empty()) {
        for(size_t i=0; i<xml_msg.size(); ++i) {
            if(!xml_msg[i].empty()) {
                msg.append(elstart).append(" phrase=\"").append(xml_msg[i]).append("\"/>");
            }
        }
    }

    if(msg.empty()) {
        msg.append(elstart).append("\"/>");
    }

    return u8vector_t(msg.begin(), msg.end());
}


//mislim da bi omogucenje rollbacka za putdoc u slucaju neuspjelog razrjesenja medjuzavisnosti
//resursa bilo prilicno paranoicno jer trebalo bi da se sve nerazrijesene zavisnosti mogu
//kasnije rijesiti buducim zahtjevima
response_code_e XCAccess::put(xcacontext_t& ctx) const
{
	ctx.reBodyOut().clear();
	ctx.reEtagOut().clear();
	ctx.reMimeOut().clear();

    //TODO: Prema RFCu pored username trebalo bi provjeriti da li postoji i XUI u storage, ako ne
    //postoji onda treba vratiti Not Found. Tek onda se provjerava ovlascenje. Vazi sa sve metode!

    //NOTE: if we are here then server succesfully authenticated 'username' but there are
    //authorization policies defined by RFC 4825 (default) or 4826, 4827  etc.
    if(!authorized(ctx.rqUsername(), ctx.rqUri().docpath.xui, WRITE_ACCESS))
        return XCAP_FAIL_AUTHORIZATION; //Unauthorized

    if(ctx.rqIfEtags().size()>1 || (ctx.rqIfEtags().size()==1 && (ctx.rqIfNoEtags().size() > 0)))
        return XCAP_FAIL_NOT_FOUND; //Not Found - undefined by rfc2616

    u8vector_t doc;//dokument na kome se vrsi izmjena
    std::string etag;//verzija dokumenta na kome se vrsi izmjena
    if(0 != getdoc(doc, etag, ctx.rqUri(), ctx.rqDomain()))
        return XCAP_ERROR_INTERNAL;

    assert((!doc.empty() && !etag.empty()) || (doc.empty() && etag.empty()));

    if(!perform_method(ctx.rqIfEtags(), ctx.rqIfNoEtags(), etag))
        return XCAP_FAIL_IF_PERFORM;

    u8vector_t docnew;//za izmjene unutar dokumenta
    rawcontent_t docactual = {0, 0};

	response_code_e rstatus_path(XCAP_OK);

    if(!ctx.rqUri().npath.empty()) {
        if(doc.empty())
            return XCAP_FAIL_NOT_FOUND;

        if(ctx.rqUri().npath.back().type == xml::nodestep_t::NODE_NAMESPACE) {
			ctx.reExtraHeadersOut()["Allow"] = "GET";
            return XCAP_FAIL_NOT_ALLOWED;
        }
        //doc+req.body => docnew pomocu xpath i xercesc dom
        rstatus_path = put_xpath(docnew, doc, ctx.rqUri().npath, ctx.rqUri().prefixes, ctx.rqBody(), ctx.rqMime());
        if((rstatus_path&(-2)) != XCAP_OK) {
            if(rstatus_path == XCAP_FAIL_CONSTRAINTS) 
				ctx.reBodyOut() = docnew;
            return rstatus_path;
        }
        docactual.content = docnew.data();
        docactual.length = docnew.size();
    } else {
        docactual = ctx.rqBody();
    }

    //8.2.2 Verifying Document Content and 8.2.5 Validation

    doctree_ptr xr_doc(XMLEngine::parse(docactual.content, docactual.length));

    assert((XMLEngine::document_validity() == xml::XML_VALID && xr_doc.get())
           || XMLEngine::document_validity() != xml::XML_VALID);

    if(XMLEngine::document_validity() != xml::XML_VALID) {
		ctx.reBodyOut() = xml_invalid_error();
        return XCAP_FAIL_CONSTRAINTS;
    }

    if(ctx.rqUri().npath.empty() //inace je vec obavljena takva provjera
       && ctx.rqMime() != app_info.mime)
        return XCAP_FAIL_MIME;
    {
		response_code_e rstatus = constraints(ctx.reBodyOut(), xr_doc, ctx.rqUri(), doc);//defined by subtype
        if((rstatus&(-2)) != XCAP_OK) return rstatus;
    }
	
	ctx.reEtagOut() = boost::uuids::to_string(boost::uuids::random_generator()());//novi etag

    if(putdoc(ctx.rqUri()
              , docactual
              , ctx.reEtagOut()
              , etag
              , ctx.rqDomain()) != 0) //NOTE: etag.empty() ? Storage insert : Storage update
        return XCAP_ERROR_INTERNAL;

    //TODO: 8.2.7 Resource Interdependencies

    if(ctx.rqUri().npath.empty())
        return etag.empty() ? XCAP_OK_CREATED : XCAP_OK;

    return rstatus_path;
}

response_code_e XCAccess::del(xcacontext_t& ctx) const
{
	ctx.reBodyOut().clear();
	ctx.reEtagOut().clear();
	ctx.reMimeOut().clear();

    if(!authorized(ctx.rqUsername(), ctx.rqUri().docpath.xui, WRITE_ACCESS))
        return XCAP_FAIL_AUTHORIZATION; //Not Found

    if(ctx.rqIfEtags().size() > 0 && ctx.rqIfNoEtags().size() > 0)
        return XCAP_FAIL_NOT_FOUND; //Not Found, undefined by rfc2616

    u8vector_t doc;//dokument na kome se vrsi izmjena
    std::string etag;//verzija dokumenta na kome se vrsi izmjena
    if(getdoc(doc, etag, ctx.rqUri(), ctx.rqDomain()) != 0)
        return XCAP_ERROR_INTERNAL;

    assert((!doc.empty() && !etag.empty()) || (doc.empty() && etag.empty()));

    if(doc.empty())
        return XCAP_FAIL_NOT_FOUND;//not found

    if(!perform_method(ctx.rqIfEtags(), ctx.rqIfNoEtags(), etag))
        return XCAP_FAIL_IF_PERFORM;

    if(ctx.rqUri().npath.empty()) { //citav dokument
        if(deldoc(ctx.rqUri(), etag, ctx.rqDomain()) != 0)
            return XCAP_ERROR_INTERNAL;
        //As long as the document still exists after the delete operation, any successful response
        //to DELETE MUST include the entity tag of the document.
		ctx.reEtagOut().clear();//indikacija da dokument vise ne postoji tj. ne treba etag u odzivu iako je XCAP_OK
        return XCAP_OK;
    }

    assert(!etag.empty());

    if(ctx.rqUri().npath.back().type == xml::nodestep_t::NODE_NAMESPACE) {
		ctx.reExtraHeadersOut()["Allow"] = "GET";
        return XCAP_FAIL_NOT_ALLOWED;
    }

    u8vector_t docnew;
	response_code_e rstatus = del_xpath(docnew, doc, ctx.rqUri().npath, ctx.rqUri().prefixes);
    if((rstatus&(-2)) != XCAP_OK) {
        if(rstatus == XCAP_FAIL_CONSTRAINTS)
			ctx.reBodyOut() = docnew;
        return rstatus;
    }

    DBGMSGAT("Working on whole document");

    //NOTE: Provjera da li se istim pathom oznacava neki drugi element je obavljena u del_xpath,
    // mislim da nije bitno sto je obavljena prije finalne validacije kako je trazeno u RFC 4825

    doctree_ptr xr_doc(XMLEngine::parse(docnew.data(), docnew.size()));//validate xml
    assert((XMLEngine::document_validity() == xml::XML_VALID && xr_doc.get())
           || XMLEngine::document_validity() != xml::XML_VALID);

    if(XMLEngine::document_validity() != xml::XML_VALID) {
		ctx.reBodyOut() = xml_invalid_error();
        return XCAP_FAIL_CONSTRAINTS;
    }

    rstatus = constraints(ctx.reBodyOut(), xr_doc, ctx.rqUri(), doc);//defined by subtype
    if((rstatus&(-2)) != XCAP_OK)  return rstatus;//trebalo bi da je XCAP_FAIL_CONSTRAINTS ili XCAP_OK

	ctx.reEtagOut() = boost::uuids::to_string(boost::uuids::random_generator()());//novi etag
    rawcontent_t docnewwrapp = { docnew.data(), docnew.size() };
    if(putdoc(ctx.rqUri()
              , docnewwrapp
              , ctx.reEtagOut()//novi etag
              , etag
              , ctx.rqDomain()) != 0)
        return XCAP_ERROR_INTERNAL;

    //TODO: 8.2.7 Resource Interdependencies

    return XCAP_OK;
}

void XCAccess::handle_request(xcacontext_t& ctx) const
{
    try {
        switch(ctx.rqMethod()) {
        case xcacontext_t::RQ_METHOD_GET:
			ctx.reCode(get(ctx));
            break;
        case xcacontext_t::RQ_METHOD_PUT:
			ctx.reCode(put(ctx));
            break;
        case xcacontext_t::RQ_METHOD_DELETE:
			ctx.reCode(del(ctx));
            break;
        }
    } catch(storage_error const& ex) {
		ctx.reCode(XCAP_ERROR_INTERNAL);
        std::wclog << "For URI '" << printable(ctx.rqUri().docpath)
                  << "' encountered storage error:\n"
                  << boost::diagnostic_information(ex)
                  << std::endl;
    } catch (invalid_stored_document const& ex) {
        //NOTE: dokument je uzet iz storage sto znaci ako nije uspjelo parsiranje onda
        // se u bazi nalazi los dokument. Znaci potrebna je intervencija administratora baze.
		ctx.reCode(XCAP_ERROR_INTERNAL);
        std::wclog << "For URI '" << printable(ctx.rqUri().docpath)
                  << "' encountered invalid_stored_document:\n"
                  << boost::diagnostic_information(ex)
                  << std::endl;
    }
}

}
}
