#include "scarlet/xml/XMLSelect.h"
#include "XMLUni.h"
#include "bmu/itemlist_iterator.h"
#include "bmu/Logger.h"
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/util/XMLUniDefs.hpp>

/** Specjalizacija za DOMNodeList (getLength za duzinu liste i item(index) za elemente liste. */
template <>
struct bmu::itemlist_traits<xercesc::DOMNodeList const, xercesc::DOMNode*>
{
	static size_t size(xercesc::DOMNodeList const& __c) { return __c.getLength(); }
	static xercesc::DOMNode* item(xercesc::DOMNodeList const& __c, size_t idx) { return __c.item(idx); }
};

/** Specjalizacija za DOMNamedNodeMap (getLength za duzinu liste i item(index) za elemente liste. */
template <>
struct bmu::itemlist_traits<xercesc::DOMNamedNodeMap const, xercesc::DOMNode*>
{
	static size_t size(xercesc::DOMNamedNodeMap const& __c) { return __c.getLength(); }
	static xercesc::DOMNode* item(xercesc::DOMNamedNodeMap const& __c, size_t idx) { return __c.item(idx); }
};

namespace scarlet {

namespace xml {

bool match_element_type::operator()(xercesc::DOMNode const* node) const
{
    return node->getNodeType() == xercesc::DOMNode::ELEMENT_NODE;
}

bool match_prefixed_name::operator()(xercesc::DOMNode const* node) const
{
    switch(node->getNodeType()) {
    case xercesc::DOMNode::ELEMENT_NODE:
    case xercesc::DOMNode::ATTRIBUTE_NODE:
        break;
    default:
        return false;
    }
    u8vector_t tmp;
    if(node->getNodeType() == xercesc::DOMNode::ELEMENT_NODE) {
        XMLCh const* const ns_uri(node->getNamespaceURI());
        if(!_TRUTF8(tmp, ns_uri, xercesc::XMLString::stringLen(ns_uri))) return false;//not-utf8
        if(stepprefix.empty()) {
            if(default_ns != tmp) return false;
        } else {
            nsbindings_it it(const_cast<nsbindings_t&>(prefixes).find(stepprefix));
            if(it == prefixes.end() || it->second != tmp) return false;
        }
    }
    if(steplocalname != (u8unit_t*)"*") {//not wildcarded
        XMLCh const* const local_name(node->getLocalName());
        if(!_TRUTF8(tmp, local_name, xercesc::XMLString::stringLen(local_name))) return false;//not-utf8
        if(steplocalname != tmp) return false;
    }
    return true;
}

//true ako ima bar jedan atribut sa zadanim prefiksom imenom i vrijednoscu
bool match_attvalue::operator()(xercesc::DOMNode const* el_node) const
{
    assert(el_node->getNodeType() == xercesc::DOMNode::ELEMENT_NODE);
    xercesc::DOMNamedNodeMap const* const attribs(el_node->getAttributes());
    for(size_t i=0, nattr=attribs->getLength(); i<nattr; ++i) {
        xercesc::DOMNode* attr(attribs->item(i));
        if(!same_name(attr)) continue;
        u8vector_t tmp;
        XMLCh const* const value(attr->getNodeValue());
        if(!_TRUTF8(tmp, value, xercesc::XMLString::stringLen(value))) return false;//not-utf8
        if(attvalue == tmp) return true;
    }
    return false;
}

//ovo je copy_if za __GXX_EXPERIMENTAL_CXX0X__
template<typename _InputIterator, typename _OutputIterator,
   typename _Predicate>
_OutputIterator
copyto_if(_InputIterator __first, _InputIterator __last,
    _OutputIterator __result, _Predicate __pred)
{
#   ifdef _GLIBCXX_CONCEPT_CHECKS
  // concept requirements
  __glibcxx_function_requires(_InputIteratorConcept<_InputIterator>)
  __glibcxx_function_requires(_OutputIteratorConcept<_OutputIterator,
    typename iterator_traits<_InputIterator>::value_type>)
  __glibcxx_function_requires(_UnaryPredicateConcept<_Predicate,
    typename iterator_traits<_InputIterator>::value_type>)
  __glibcxx_requires_valid_range(__first, __last);
#   endif
  for (; __first != __last; ++__first)
if (__pred(*__first))
  {
    *__result = *__first;
    ++__result;
  }
  return __result;
}

void XMLSelect::select(std::vector<xercesc::DOMNode*>& out
            , xercesc::DOMNode* context
            , nodestep_t const& step
            , nsbindings_t const& prefixes
            , u8vector_t const& default_ns)
{
    assert(context);
    assert(out.empty());
    DBGMSGAT("Selecting step: prefix='" << bmu::utf8_string(step.prefix) << "' and name='" << bmu::utf8_string(step.localname) << "'");
    switch(step.type) {
    case nodestep_t::NODE_ELEMENT: {
        xercesc::DOMNodeList* allnodes(context->getChildNodes());
        assert(allnodes);
        DBGMSGAT("Working on list of " << allnodes->getLength() << " elements");
//#ifndef NDEBUG
//        for(size_t i=0; i<allnodes->getLength(); ++i) XMLSelect::dump(allnodes->item(i));
//#endif
        std::vector<xercesc::DOMNode*> allnamed;
        {
			bmu::itemlist_iterator<xercesc::DOMNodeList const, xercesc::DOMNode*> itall(*allnodes);
            copyto_if(itall, itall.end(), std::back_inserter(allnamed)
                      , match_prefixed_name(step, default_ns, prefixes));
        }
        DBGMSGAT("Found " << allnamed.size() << " elements: prefix:name='"
                << bmu::utf8_string(step.prefix) << ":" << bmu::utf8_string(step.localname) << "'");
//#ifndef NDEBUG
//        for(size_t i=0; i<allnamed.size(); ++i) XMLSelect::dump(allnamed[i]);
//#endif
        if(step.position > 0) {
            if(step.position > allnamed.size()) {
                DBGMSGAT("Requested position is to high, no-match");
                return;//no-match
            }
            DBGMSGAT("Getting element at " << step.position-1);
            xercesc::DOMNode* tmpnode(allnamed[step.position-1]);
            allnamed.clear();
            allnamed.push_back(tmpnode);
//#ifndef NDEBUG
//            XMLSelect::dump(tmpnode);
//#endif
        }
        if(!step.attvalue.empty()) {
            DBGMSGAT("Getting element with attribute: @name=value = '@" << bmu::utf8_string(step.attname)
                    << "=" << bmu::utf8_string(step.attvalue) << "'");
            copyto_if(allnamed.begin(), allnamed.end(), std::back_inserter(out)
                      , match_attvalue(step, default_ns, prefixes));
        } else {
            DBGMSGAT("Getting all named elements");
            out.assign(allnamed.begin(), allnamed.end());
        }
    } break;
    case nodestep_t::NODE_ATTRIBUTE: {
        assert(context->getNodeType() == xercesc::DOMNode::ELEMENT_NODE);
        xercesc::DOMNamedNodeMap const* const attribs(context->getAttributes());
        DBGMSGAT("Working on list of " << attribs->getLength() << " attributes");
#ifndef NDEBUG
        for(size_t i=0; i<attribs->getLength(); ++i) XMLSelect::dump(attribs->item(i));
#endif
		bmu::itemlist_iterator<xercesc::DOMNamedNodeMap const, xercesc::DOMNode*> itall(*attribs);
        copyto_if(itall, itall.end(), std::back_inserter(out)
                  , match_prefixed_name(step, default_ns, prefixes));
        DBGMSGAT("Found " << out.size() << " named attributes");

    } break;
    default: {
        assert(0);//Should never be executed
    } break;
    }
#ifndef NDEBUG
    for(size_t i=0; i<out.size(); ++i) XMLSelect::dump(out[i]);
#endif
}

XMLSelect::XMLSelect(doctree_ptr const& xr_doc)
 : obj()
{
    assert(xr_doc.get());
    xercesc::DOMNode* subroot(xr_doc->getDocumentElement());
    if(subroot) obj.push_back(subroot);
}

XMLSelect::XMLSelect(doctree_ptr const& xr_doc
                     , std::vector<nodestep_t> const& steps
                     , size_t count
                     , nsbindings_t const& prefixes
                     , u8vector_t const& default_ns)
 : obj()
{
    if(steps.empty() || count == 0 || count > steps.size()) {
        DBGMSGAT("Nothing to query, nsteps=" << steps.size() << ", count=" << count);
        return;
    }
    assert(xr_doc.get());
    xercesc::DOMNode* subroot(xr_doc->getDocumentElement());
    assert(subroot);
    if(steps[0].type != nodestep_t::NODE_ELEMENT
       || steps[0].position>1
       || !match_prefixed_name(steps[0], default_ns, prefixes)(subroot)
       || (!steps[0].attvalue.empty()
           && !match_attvalue(steps[0], default_ns, prefixes)(subroot)))
        return;
    std::vector<xercesc::DOMNode*> tmp;
    tmp.push_back(subroot);
    for(size_t i=1; i<count; ++i) {
        tmp.clear();
        select(tmp, subroot, steps[i], prefixes, default_ns);
        if(tmp.size() != 1) {
            DBGMSGAT("Requested step is no-match (multiple or no node)");
            return; //no-match
        }
        subroot = tmp[0];
    }
    obj.assign(tmp.begin(), tmp.end());
}

//selected children
XMLSelect::XMLSelect(XMLSelect const& context
                     , nodestep_t const& step
                     , nsbindings_t const& prefixes
                     , u8vector_t const& default_ns)
 : obj()
{
    assert(context.node());
    select(obj, context.node(), step, prefixes, default_ns);
}

//named children
XMLSelect::XMLSelect(XMLSelect const& context
                     , u8vector_t const& stepprefix
                     , u8vector_t const& stepname
                     , nsbindings_t const& prefixes
                     , u8vector_t const& default_ns)
 : obj()
{
    assert(context.node());
    xercesc::DOMNodeList* allnodes(context.node()->getChildNodes());
    assert(allnodes);
	bmu::itemlist_iterator<xercesc::DOMNodeList const, xercesc::DOMNode*> itall(*allnodes);
    if(stepname.empty()) { //allchildren
        copyto_if(itall, itall.end(), std::back_inserter(obj), match_element_type());
    } else { //named children
        copyto_if(itall, itall.end(), std::back_inserter(obj)
                  , match_prefixed_name(stepname, default_ns, stepprefix, prefixes));
    }
}

void XMLSelect::dump(xercesc::DOMNode* xr_node)
{
#ifndef NDEBUG
    assert(xr_node);
    DBGMSGAT("Node name: " << _TRLCP(xr_node->getNodeName())
              << "\n\tNamespace URI: " << _TRLCP(xr_node->getNamespaceURI())
              << "\n\tLocal name: " << _TRLCP(xr_node->getLocalName())
              << "\n\tPrefix: " << _TRLCP(xr_node->getPrefix())
              << "\n\tValue: '" << _TRLCP(xr_node->getNodeValue()) << "'");

    if(xr_node->getNodeType() == xercesc::DOMNode::ELEMENT_NODE
       && static_cast<xercesc::DOMElement*>(xr_node)->hasAttributes()) {
        xercesc::DOMNamedNodeMap* attr(static_cast<xercesc::DOMElement*>(xr_node)->getAttributes());
        for(size_t i=0; i<attr->getLength(); ++i) {
            DBGMSG("\t\tAttribute "
                  << _TRLCP(attr->item(i)->getNodeName())
                  << "='"
                  << _TRLCP(attr->item(i)->getNodeValue()) << "'");
        }
    }
#endif
}

xercesc::DOMNode* XMLSelect::node_parent(void) const
{
    xercesc::DOMNode* xr_node(node());
    return xr_node ? xr_node->getParentNode() : 0;
}

void XMLSelect::remove(void)
{
    xercesc::DOMNode* xr_node(node());
    xercesc::DOMNode* xr_parent(node_parent());
    assert(xr_parent && xr_node);
    //Uz element se dijelom uklanjaju i bjeline kojima je okruzen da se ne povecavaju praznine
    //u dokumentu kad se elementi ubacuju pa brisu. Uklanja se sve iza \n u prethodnom text cvoru
    //i sve do \n, ukljucujuci ga, u iducem cvoru.
    xercesc::DOMNode* before(xr_node->getPreviousSibling());
    if(before) {
        DBGMSGAT("Node before node we removing:");
        XMLSelect::dump(before);
        if(before->getNodeType() == xercesc::DOMNode::TEXT_NODE) {
            XMLCh const* txt(before->getNodeValue());
            if(txt) {
                size_t len(std::char_traits<XMLCh>::length(txt));
                XMLCh const* const txtend(txt+len);
                XMLCh const* lfpos(std::find(txt, txtend, xercesc::chLF));
                if(++lfpos<txtend) {
                    before->setNodeValue(xmlstring(txt, lfpos).c_str());
                }
            }
        }
    }
    xercesc::DOMNode* after(xr_node->getNextSibling());
    if(after) {
        DBGMSGAT("Node after node we removing:");
        XMLSelect::dump(after);
        if(after->getNodeType() == xercesc::DOMNode::TEXT_NODE) {
            XMLCh const* txt(after->getNodeValue());
            if(txt) {
                size_t len(std::char_traits<XMLCh>::length(txt));
                XMLCh const* const txtend(txt+len);
                XMLCh const* lfpos(std::find(txt, txtend, xercesc::chLF));
                if(lfpos<txtend) {
                    after->setNodeValue(xmlstring(++lfpos, txtend).c_str());
                }
            }
        }
    }
    xr_parent->removeChild(xr_node);
    //wrapper vise ne vrijedi
    obj.clear();
}

xercesc::DOMNode* XMLSelect::attribute(u8vector_t const& name) const
{
    xmlstring attname;
    if(!_TRUTF8(attname, name.data(), name.size()))
        return 0;

    xercesc::DOMElement* element(static_cast<xercesc::DOMElement*>(node()));
    return static_cast<xercesc::DOMNode*>(element->getAttributeNode(attname.c_str()));
}

xercesc::DOMNode* XMLSelect::attribute(u8vector_t const& name, rawcontent_t const& value) const
{
    xercesc::DOMElement* element(static_cast<xercesc::DOMElement*>(node()));
    assert(element);

    xmlstring attname;
    xmlstring attvalue;

    if(!_TRUTF8(attname, name.data(), name.size())
       || !_TRUTF8(attvalue, value.content, value.length))
        return 0;

    try {
        element->setAttribute(attname.c_str(), attvalue.c_str());
        return static_cast<xercesc::DOMNode*>(element->getAttributeNode(attname.c_str()));
    } catch (...) {
        xml_engine_exception_handler();
    }
    return 0;
}

void XMLSelect::value(u8vector_t& xml_str) const
{
    assert(node());
    XMLCh const* node_value(node()->getNodeValue());
    if(node_value) {
        bool status = _TRUTF8(xml_str, node_value);
        assert(status); //NOTE: prije poziva value() obavezno provjeri da li je valjan UTF-8
    }
}

XMLSelect XMLSelect::first_child(void) const
{
    assert(node());
    return XMLSelect(node()->getFirstChild());
}

XMLSelect XMLSelect::next_sibling(void) const
{
    assert(node());
    return XMLSelect(node()->getNextSibling());
}

/* The scope of a default namespace declaration extends from the beginning of the start-tag in which
it appears to the end of the corresponding end-tag, excluding the scope of any inner default
namespace declarations.
The scope of a namespace declaration declaring a prefix extends from the beginning of the start-tag
in which it appears to the end of the corresponding end-tag, excluding the scope of any inner
declarations with the same NSAttName part.*/
void XMLSelect::namespace_bindings(u8vector_t& xml_str) const
{
    xercesc::DOMNode const* thisNode(node());
    std::map<xmlstring, xmlstring> pfx_ns;
    DBGMSGAT("Searching namespace bindings ...");
    for(xercesc::DOMNode const* current = thisNode; current; current = current->getParentNode()) {
        if(!current->hasAttributes()) continue;
        xercesc::DOMNamedNodeMap* nodeMap = current->getAttributes();
        if(nodeMap == 0) continue;
        size_t length = nodeMap->getLength();
        for (XMLSize_t i = 0;i < length;i++) {
            xercesc::DOMNode *attr = nodeMap->item(i);
            const XMLCh* ns = attr->getNamespaceURI();
            if (ns == 0 || !xercesc::XMLString::equals(ns, xercesc::XMLUni::fgXMLNSURIName))
                continue;
            const XMLCh *value = attr->getNodeValue();
            const XMLCh *attrName = attr->getNodeName();//fullname pfx:local
            if (xercesc::XMLString::equals(attrName, xercesc::XMLUni::fgXMLNSString)) {
                if(pfx_ns.find(xmlstring()) == pfx_ns.end())
                    pfx_ns[xmlstring()] = value;// default namespace
                continue;
            }
            const XMLCh *attrPrefix = attr->getPrefix();
            const XMLCh *attrLocalName = attr->getLocalName();
            if (attrPrefix != 0
                       && xercesc::XMLString::equals(attrPrefix, xercesc::XMLUni::fgXMLNSString)) {
                if(pfx_ns.find(attrLocalName) == pfx_ns.end())
                    pfx_ns[attrLocalName] = value;// non default namespace
            }
        }
    }
    xmlstring all;
    all.push_back(xercesc::chOpenAngle);
    all.append(thisNode->getNodeName());
    for(std::map<xmlstring, xmlstring>::iterator it=pfx_ns.begin(); it != pfx_ns.end(); ++it) {
        all.push_back(xercesc::chSpace);
        all.append(xercesc::XMLUni::fgXMLNSString);
        if(!it->first.empty()) {
            all.push_back(xercesc::chColon);
            all.append(it->first);
        }
        all.push_back(xercesc::chEqual);
        all.push_back(xercesc::chDoubleQuote);
        all.append(it->second);
        all.push_back(xercesc::chDoubleQuote);
    }
    all.push_back(xercesc::chForwardSlash);
    all.push_back(xercesc::chCloseAngle);
    if(!_TRUTF8(xml_str, all))
        DBGMSGAT("Failed converting bindings to UTF-8");
}

}
}