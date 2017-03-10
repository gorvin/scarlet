#ifndef XML_SELECT_H
#define XML_SELECT_H

#include <bmu/tydefs.h>
#include <scarlet/xml/xmldefs.h>
#include <map>
#include <vector>

namespace scarlet {
namespace xml {
using ::bmu::u8vector_t;
using ::bmu::u8vector_it;
using ::bmu::rawcontent_t;

/** Predstavlja jedan korak selektora ?vora. */
struct nodestep_t {
    enum node_type_e {
        NODE_ELEMENT,
        NODE_ATTRIBUTE,
        NODE_NAMESPACE,
        NODE_EXTENSION
    }           type;
    u8vector_t steppath; //full step
    u8vector_t prefix;//prefix elementa ili atributa
    u8vector_t localname;//ime elementa (ili '*') ili ime atributa
    bool       wildcarded;
    size_t     position;//positional predicate value (zero if no positional predicate)
    u8vector_t attprefix;//attributive predicate attribute name
    u8vector_t attname;//attributive predicate attribute name
    u8vector_t attvalue;//attributive predicate attribute value

    nodestep_t(void)
     : type(NODE_ELEMENT)
     , steppath()
     , prefix()
     , localname()
     , wildcarded(false)
     , position(0)
     , attprefix()
     , attname()
     , attvalue()
     { }
};

/** Uparivanje prefiksa u prostore imena. Koristi se za predstavljanje niza xmlns upita iz XCAP
URIja ili pri radu sa XML dokumentom.
*/
typedef std::map<u8vector_t, u8vector_t> nsbindings_t;
typedef nsbindings_t::iterator           nsbindings_it;


struct match_element_type : public std::unary_function<xmlnode_t*, bool> {
    bool operator()(xmlnode_t const* node) const;
};


class match_prefixed_name : public std::unary_function<xmlnode_t*, bool> {
    u8vector_t const stepprefix;
    u8vector_t const steplocalname;
    nsbindings_t const prefixes;
    u8vector_t const default_ns;

public:
    bool operator()(xmlnode_t const* node) const;

    u8vector_t const& el_pfx(void) const { return stepprefix; }
    u8vector_t const& el_name(void) const { return steplocalname; }
    u8vector_t const& el_ns(void) const { return default_ns; }

    match_prefixed_name(
        nodestep_t const& step
        , u8vector_t const& default_ns
        , nsbindings_t const& prefixes = nsbindings_t()
    )
     : stepprefix(step.prefix)
     , steplocalname(step.localname)
     , prefixes(prefixes)
     , default_ns(default_ns)
     { }

    match_prefixed_name(
        u8vector_t const& steplocalname
        , u8vector_t const& default_ns
        , u8vector_t const& stepprefix = u8vector_t()
        , nsbindings_t const& prefixes = nsbindings_t()
    )
     : stepprefix(stepprefix)
     , steplocalname(steplocalname)
     , prefixes(prefixes)
     , default_ns(default_ns)
     { }
};


class match_attvalue : public std::unary_function<xmlnode_t*, bool> {
    match_prefixed_name same_name;
    u8vector_t const attvalue;
    nsbindings_t const prefixes;
    u8vector_t const default_ns;

public:
    //true ako ima bar jedan atribut sa zadanim prefiksom imenom i vrijednoscu
    bool operator()(xmlnode_t const* el_node) const;

    match_attvalue(
        nodestep_t const& step
        , u8vector_t const& default_ns
        , nsbindings_t const& prefixes
    )
     : same_name(step.attname, default_ns, step.attprefix, prefixes)
     , attvalue(step.attvalue)
     , prefixes(prefixes)
     , default_ns(default_ns)
     { }
};

typedef boost::shared_ptr<xmldoc_t> doctree_ptr;

/** Trazenje objekata objekata DOM stabla i wrapper rezultata. */
class XMLSelect {
    std::vector<xmlnode_t*> obj;

    explicit XMLSelect(void); //NE

    static void dump(xmlnode_t* xr_node);

    //selected direct children
    static void select(
        std::vector<xmlnode_t*>& out
        , xmlnode_t* context
        , nodestep_t const& step
        , nsbindings_t const& prefixes
        , u8vector_t const& default_ns
    );
public:
    //select document element (root node)
    XMLSelect(doctree_ptr const& xr_doc);

    //explicit set node as selected
    XMLSelect(xmlnode_t* xr_node)
     : obj()
    {
        if(xr_node) obj.push_back(xr_node);
    }

    //selected descendant nodes, each step until last must select unique child node of previous node
    XMLSelect(doctree_ptr const& xr_doc
              , std::vector<nodestep_t> const& steps
              , size_t count
              , nsbindings_t const& prefixes
              , u8vector_t const& default_ns);

    //selected direct children
    XMLSelect(XMLSelect const& context
              , nodestep_t const& step
              , nsbindings_t const& prefixes
              , u8vector_t const& default_ns);

    //named direct children or if name empty all direct children
    XMLSelect(XMLSelect const& context
              , u8vector_t const& prefix
              , u8vector_t const& name
              , nsbindings_t const& prefixes
              , u8vector_t const& default_ns);

    size_t count(void) const { return obj.size(); }

    xmlnode_t* node(void) const { return (count() == 1) ? obj.front() : 0; }

    xmlnode_t* operator[](size_t idx) const { return obj[idx]; }
    xmlnode_t* node_at(size_t idx) const { return (idx >= count()) ? 0 : obj[idx]; }

    void node_at_dump(size_t idx) const
    {
        XMLSelect::dump(node_at(idx));
    }

    /* If an attribute with that name is already present in the element, its value is changed
       to be that of the value parameter */
    xmlnode_t* attribute(u8vector_t const& name, rawcontent_t const& value) const;
    xmlnode_t* attribute(u8vector_t const& name) const;

    void value(u8vector_t& xml_str) const;

    XMLSelect first_child(void) const;
    XMLSelect next_sibling(void) const;

    void namespace_bindings(u8vector_t& xml_str) const;

    xmlnode_t* node_parent(void) const;//CHECK: ovo treba samo za remove
    void remove(void);
};

}
}

#endif // XML_SELECT_H
