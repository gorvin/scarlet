#include "scarlet/xcap/XMLEngine.h"
#include "bmu/LexerChars.h"
#include "bmu/Logger.h"
#include <xercesc/dom/DOMElement.hpp>
#include "bmu/exdefs.h"

namespace scarlet {
namespace xcap {

//std::string GetLocaleString(void) { return "en_US";  }


XMLEngine::XMLEngine(xml::XercesScopePtr xerces_scope
                     , std::string const& nsxsd_map
                     , std::string const& xsd_dir)
 : tree(new xml::XMLTree(xerces_scope, nsxsd_map, xsd_dir))
 , lsio(new xml::XMLFragment(xerces_scope))
 , subtree(new xml::XMLSubtree(xerces_scope))
{
    assert(tree.get());
    assert(lsio.get());
    assert(subtree.get());
}

XMLEngine::~XMLEngine() { }

doctree_ptr XMLEngine::parse(u8unit_t const* xml_str, size_t xml_str_size) const
{
    return tree->parse(xml_str, xml_str_size);
}

xml::xml_validity_e XMLEngine::document_validity(void) const
{
    return tree->validity();
}

std::vector<std::string> const& XMLEngine::document_errors(void) const
{
    return tree->get_messages();
}

xml::xml_validity_e XMLEngine::fragment_validity(void) const
{
    return subtree->validity();
}

std::vector<std::string> const& XMLEngine::fragment_errors(void) const
{
    return subtree->get_messages();
}

void XMLEngine::dump(xml::XMLSelect const& r, u8vector_t const& used_expression) const
{
    std::cout << "Result for get [" << bmu::utf8_string(used_expression) << "]:\n";
    size_t const count(r.count());
    for(size_t i = 0; i<count; ++i) lsio->serialize(r[i]);
}

void XMLEngine::serialize(u8vector_t& xml_out, xml::XMLSelect const& r) const
{
    xercesc::DOMNode* xr_node(r.node());
    assert(xr_node);//logicka greska, ne zovi ako r nema jedinstveni cvor

    lsio->serialize(xml_out, xr_node);
}

void XMLEngine::serialize(u8vector_t& xml_out, doctree_ptr const& xr_doc) const
{
    assert(xr_doc.get());//logicka greska
    xercesc::DOMDocument* xr_root(xr_doc.get());
    lsio->serialize(xml_out, xr_root);
}

docsubtree_ptr XMLEngine::parse(
    doctree_ptr const& xr_doc
    , u8unit_t const* part
    , size_t part_size
    , u8vector_t const& default_ns
) const
{
    return subtree->parse(xr_doc, part, part_size, default_ns);
}

xercesc::DOMNode* XMLEngine::put(
    xercesc::DOMNode* near_node
    , docsubtree_ptr const& docpart
    , xml::put_node_context_t ctx
) const
{
    return subtree->put(near_node, docpart, ctx);
}

XMLMethods::XMLMethods(
	xml::XercesScopePtr xerces_scope
	, u8vector_t const& application_ns
    , std::string const& nsxsd_map
    , std::string const& xsd_dir
)
 : XMLEngine(xerces_scope, nsxsd_map, xsd_dir)
 , default_ns(application_ns)
 , predicates(new bmu::LexerChars)
{
    assert(predicates.get());
}

XMLMethods::~XMLMethods() { }

//8.2.3 [RFC 4825]
xercesc::DOMNode*
 XMLMethods::put_create(
	 xml::XMLSelect const& rparent
    , xml::nodestep_t const& nodexpath
    , xml::nsbindings_t const& prefixes
    , docsubtree_ptr const& docpart
) const
{
    DBGMSGAT("Creating element");

	xml::XMLSelect rall(rparent
                   , u8vector_t()
                   , u8vector_t()
                   , prefixes
                   , default_ns);

	xml::XMLSelect rname(rparent
                   , nodexpath.prefix
                   , nodexpath.localname
                   , prefixes
                   , default_ns);

    size_t const nallsibl(rall.count());
    size_t const nnamesibl(rname.count());
    bool const   wildcarded(nodexpath.wildcarded);
    size_t const position(nodexpath.position);

    //positionally constrained (3.)
    if(position>1) {
        if(wildcarded) DBGMSGAT("Selector is wildcarded");

        if(position>(wildcarded ? nallsibl : nnamesibl)+1) {
            DBGMSGAT("positionaly constrained but too high position");
            return 0;//<cannot-insert>
        }
        xercesc::DOMNode* near_node(wildcarded ? rall.node_at(position-2)
                                               : rname.node_at(position-2));
        DBGMSGAT("positionaly constrained acceptable position");
        if(!near_node) DBGMSG("No node to put after!!!");
        return XMLEngine::put(near_node, docpart, xml::PUT_AFTER_NODE);
    }

    //positionally constrained (3.) n=1
    if(position == 1 && wildcarded) {
        if(nallsibl>0) {
            DBGMSGAT("positionaly constrained = 1, wildcarded");
            return XMLEngine::put(rall.node_at(0), docpart, xml::PUT_BEFORE_NODE);
        }

        DBGMSGAT("positionaly constrained = 1, wildcarded, no siblings");
        return XMLEngine::put(rparent.node(), docpart, xml::PUT_APPEND_NODE);
    }

    if(position == 1 && (!wildcarded && nnamesibl>0)) {
        DBGMSGAT("positionaly constrained = 1, !wildcarded");
        return XMLEngine::put(rname.node_at(0), docpart, xml::PUT_BEFORE_NODE);
    }

    //(2.)
    if(nnamesibl > 0) {
        DBGMSGAT("after named siblings");
        return XMLEngine::put(rname.node_at(nnamesibl-1), docpart, xml::PUT_AFTER_NODE);
    }

    //(1.)
    if(nallsibl>0) {
        //NOTE:allsiblings su samo siblings tipa ELEMENT_NODE
        DBGMSGAT("after all elements");//na kraju ali ispred svih komentara, tekst cvorova i PI
        return XMLEngine::put(rall.node_at(nallsibl-1), docpart, xml::PUT_AFTER_NODE);
    }

    DBGMSGAT("appending in parent");

    return XMLEngine::put(rparent.node(), docpart, xml::PUT_APPEND_NODE);
}

response_code_e
XMLMethods::get_xpath(
    u8vector_t& docpart
    , std::string& mimetype
    , u8vector_t const& docprev
    , std::vector<xml::nodestep_t> const& nodexpath
    , xml::nsbindings_t const& prefixes
) const
{
    assert(!nodexpath.empty());

    docpart.clear();
    mimetype.clear();

    doctree_ptr xr_doc(XMLEngine::parse(docprev.data(), docprev.size()));
    //NOTE: za get dokument je uzet iz storage sto znaci ako nije uspjelo parsiranje onda
    // se u bazi nalazi los dokument. Znaci potrebna je intervencija administratora baze.
    if(XMLEngine::document_validity() != xml::XML_VALID)
        throw boost::enable_current_exception(invalid_stored_document()) << bmu::errinfo_message(
                "XML document from storage not valid!"
        );
    assert(xr_doc.get());

    size_t steps(nodexpath.size());

    if(nodexpath.back().type == xml::nodestep_t::NODE_NAMESPACE) {
        if(steps < 2) return XCAP_FAIL_NOT_FOUND;
        --steps;
    }

	xml::XMLSelect found(xr_doc
                   , nodexpath
                   , steps
                   , prefixes
                   , default_ns);

    if(!found.node()) {
        //DBGMSGAT("Not found [" << utf8_string(fullpath) << "] in document:\n" << utf8_string(docprev));
        return XCAP_FAIL_NOT_FOUND;
    }

    switch(nodexpath.back().type) {
    case xml::nodestep_t::NODE_ATTRIBUTE: {
        found.value(docpart);
        mimetype = "application/xcap-att+xml";
    } break;
    case xml::nodestep_t::NODE_ELEMENT: {
        XMLEngine::serialize(docpart, found);
        mimetype = "application/xcap-el+xml";
    } break;
    case xml::nodestep_t::NODE_NAMESPACE: {
        found.namespace_bindings(docpart);
        mimetype = "application/xcap-ns+xml";
    } break;
    default: mimetype.clear();
    }

    return XCAP_OK;
}

u8vector_t XMLMethods::fragment_invalid_error(void) const
{
    std::string elstart;

    switch(XMLEngine::fragment_validity()) {
    case xml::XML_NOT_WELL_FORMED : {
        elstart = "<not-xml-frag";
        break;
    } case xml::XML_NOT_UTF8: {
        elstart = "<not-utf8";
    } default:
        assert(0);
    }

    std::string msg;
    std::vector<std::string> const& xml_msg(XMLEngine::fragment_errors());

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

u8vector_t
 XMLMethods::percent_encoded(std::vector<xml::nodestep_t> const& vnodexpath, size_t nsteps) const
{
    u8vector_t result;

    /* copy and url encode */
    for(size_t i=0; i<nsteps; i++) {

		xml::nodestep_t const& nodexpath(vnodexpath[i]);
        result.push_back('/');

        DBGMSGAT("%encoding: " << bmu::utf8_string(nodexpath.steppath));

        size_t const nchars(nodexpath.steppath.size());
        for(size_t j=0; j<nchars; ++j) {

            u8unit_t const c(nodexpath.steppath[j]);

            if(predicates->is_NoPercent(c)) {
                result.push_back(c);
                continue;
            }

            unsigned char const hi(c>>4);
            unsigned char const lo(c&0x0f);

            result.push_back('%');
//            result.push_back(hi + ((hi<10) ? '0' : ('A'-10)));
//            result.push_back(lo + ((lo<10) ? '0' : ('A'-10)));
            static const char digits[] = "0123456789abcdef";
            result.push_back(digits[hi]);
            result.push_back(digits[lo]);
        }
    }

    return result;
}

void XMLMethods::no_parent_ancestor(
    u8vector_t& docresponse
    , doctree_ptr xr_doc
    , std::vector<xml::nodestep_t> const& vnodexpath
    , size_t nsteps
    , xml::nsbindings_t const& prefixes
) const
{
    docresponse = (u8unit_t*)"<no-parent>";

    for(size_t i=nsteps; i; --i) {

		xml::XMLSelect rtmp(xr_doc
                       , vnodexpath
                       , i - 1
                       , prefixes
                       , default_ns);

        if(rtmp.node()) {
            docresponse.append((u8unit_t*)"<ancestor>")
                       .append(percent_encoded(vnodexpath, i-1))
                       .append((u8unit_t*)"</ancestor>");
            break;
        }
    }

    docresponse.append((u8unit_t*)"<no-parent>");
}

response_code_e XMLMethods::put_xpath(
    u8vector_t& docnew
    , u8vector_t const& docprev
    , std::vector<xml::nodestep_t> const& nodexpath
    , xml::nsbindings_t const& prefixes
    , rawcontent_t const& docpart
    , std::string const& mimetype
) const
{
    assert(!nodexpath.empty());

    docnew.clear();

    doctree_ptr xr_doc(XMLEngine::parse(docprev.data(), docprev.size()));
    if(XMLEngine::document_validity() != xml::XML_VALID)
        throw boost::enable_current_exception(invalid_stored_document()) << bmu::errinfo_message(
                "XML document from storage not valid!"
        );
    assert(xr_doc.get());

    DBGMSGAT("Selecting parent");
	xml::XMLSelect rctx(xr_doc
                   , nodexpath
                   , nodexpath.size() - 1 //parent
                   , prefixes
                   , default_ns);

    if(!rctx.node()) {
        DBGMSGAT("In document" << bmu::utf8_string(docprev));
        no_parent_ancestor(docnew, xr_doc, nodexpath, nodexpath.size()-1, prefixes);
        return XCAP_FAIL_CONSTRAINTS;
    }

    xercesc::DOMNode* newnode(0);
    xercesc::DOMNode* target(0);

    switch(nodexpath.back().type) {

    case xml::nodestep_t::NODE_ATTRIBUTE: {//docpart is value of attribute named nodexpath.back().localname

        if(!predicates->is_valid_attvalue(u8vector_t(docpart.content, docpart.length))) {
            if(xml::is_valid_utf8(docpart.content, docpart.length)) {
                docnew = (u8unit_t*)"<not-xml-att-value phrase=\"Can't put body of request as "
                    "attribute value because it's not valid by XML specification\"/>";
            } else {
                docnew = (u8unit_t*)"<not-utf8 phrase=\"Can't put body of request as "
                    "attribute value because it contains invalid UTF-8 characters\"/>";
            }
            return XCAP_FAIL_CONSTRAINTS;
        }

        if(mimetype != "application/xcap-att+xml") return XCAP_FAIL_MIME;

        target = rctx.attribute(nodexpath.back().localname);//samo kao indikacija replace/create
        newnode = rctx.attribute(nodexpath.back().localname, docpart);

    } break;

    case xml::nodestep_t::NODE_ELEMENT: {

        DBGMSGAT("Selecting target element");
		xml::XMLSelect rtarget(rctx
                          , nodexpath.back()
                          , prefixes
                          , default_ns);

        DBGMSGAT("Selected " << rtarget.count() << " target elements");
        if(rtarget.count() > 1) return XCAP_FAIL_NOT_FOUND;//invalid target
        target = rtarget.node();

        docsubtree_ptr holder(XMLEngine::parse(xr_doc, docpart.content, docpart.length, default_ns));

        assert((XMLEngine::fragment_validity() == xml::XML_VALID && holder.get())
               || XMLEngine::fragment_validity() != xml::XML_VALID);

        if(XMLEngine::fragment_validity() != xml::XML_VALID) {
            docnew = fragment_invalid_error();
            return XCAP_FAIL_CONSTRAINTS;
        }

        if(mimetype != "application/xcap-el+xml")
            return XCAP_FAIL_MIME;

        newnode = target ? XMLEngine::put(target, holder, xml::PUT_REPLACE_NODE)
                         : put_create(rctx, nodexpath.back(), prefixes, holder);
    } break;

    default:
        DBGMSGAT("ERROR - Should never be executed");
        return XCAP_FAIL_NOT_ALLOWED;
    }

    if(!newnode) {
        docnew = (u8unit_t*) (target ? "<cannot-replace/>" : "<cannot-insert/>");
        return XCAP_FAIL_CONSTRAINTS;
    }

    //Povjera da li je umetnje/zamjena uspjesno i da li se ubuduce tim xpathom pokazuje mijenjani cvor
    DBGMSGAT("Checking idempotency");
	xml::XMLSelect rcheck(xr_doc
                   , nodexpath
                   , nodexpath.size()
                   , prefixes
                   , default_ns);

    if(rcheck.node() != newnode) {
        DBGMSGAT("Not idempotent");
        docnew = (u8unit_t*) (target ? "<cannot-replace": "<cannot-insert");
        docnew.append((u8unit_t*) " phrase=\"Idempotency error\"/>");
        return XCAP_FAIL_CONSTRAINTS;
    }

    XMLEngine::serialize(docnew, xr_doc);
    return target ? XCAP_OK : XCAP_OK_CREATED;
}

response_code_e XMLMethods::del_xpath(
    u8vector_t& docnew
    , u8vector_t const& docprev
    , std::vector<xml::nodestep_t> const& nodexpath
    , xml::nsbindings_t const& prefixes
) const
{
    assert(!nodexpath.empty());

    docnew.clear();

    doctree_ptr xr_doc(XMLEngine::parse(docprev.data(), docprev.size()));
    if(XMLEngine::document_validity() != xml::XML_VALID)
        throw boost::enable_current_exception(invalid_stored_document()) << bmu::errinfo_message(
                "XML document from storage not valid!"
        );
    assert(xr_doc.get());
	xml::XMLSelect found(xr_doc
                   , nodexpath
                   , nodexpath.size()
                   , prefixes
                   , default_ns);
    if(!found.node()) return XCAP_FAIL_NOT_FOUND;
    found.remove();//CHECK: Valja li ovo i za atribute?
    //Povjera da li se ubuduce tim xpathom ista pokazuje
	xml::XMLSelect rcheck(xr_doc
                     , nodexpath
                     , nodexpath.size()
                     , prefixes
                     , default_ns);
    if(rcheck.count()>0) {
        docnew = (u8unit_t*)"<cannot-delete/>";
        return XCAP_FAIL_CONSTRAINTS;
    }
    XMLEngine::serialize(docnew, xr_doc);
    return XCAP_OK;
}

}
}
