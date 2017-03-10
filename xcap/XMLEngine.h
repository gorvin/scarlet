#include "scarlet/xml/XMLBase.h"
#include "scarlet/xcap/xcadefs.h"
#include <boost/filesystem.hpp>
#include <boost/scoped_ptr.hpp>

namespace XERCES_CPP_NAMESPACE {
class DOMNode;
class DOMDocument;
class DOMDocumentFragment;
}

namespace beam_me_up {
struct LexerChars;
}

namespace scarlet {
namespace xml {
class xml_engine_t;
class XMLTree;
class XMLFragment;
class XMLSubtree;
class XMLSelect;
}

namespace xcap {

typedef boost::shared_ptr<xercesc::DOMDocument> doctree_ptr;
typedef boost::shared_ptr<xercesc::DOMDocumentFragment> docsubtree_ptr;

class XMLEngine {
    explicit XMLEngine(void) = delete; //NE
    XMLEngine& operator=(XMLEngine const&) = delete; //NE
public:
    doctree_ptr parse(u8unit_t const* xml_str, size_t xml_str_size) const;
    xml::xml_validity_e document_validity(void) const;
    xml::xml_validity_e fragment_validity(void) const;
    std::vector<std::string> const& document_errors(void) const;
    std::vector<std::string> const& fragment_errors(void) const;
    void dump(xml::XMLSelect const& r, u8vector_t const& used_expression) const;
    //Samo jedinstveni cvor
    void serialize(u8vector_t& xml_out, xml::XMLSelect const& r) const;
    void serialize(u8vector_t& xml_out, doctree_ptr const& xr_doc) const;
    docsubtree_ptr parse(doctree_ptr const& xr_doc, u8unit_t const* part
		, size_t part_size, u8vector_t const& default_ns) const;
    xercesc::DOMNode* put(xercesc::DOMNode* near_node, docsubtree_ptr const& docpart, xml::put_node_context_t ctx) const;
    XMLEngine(xml::XercesScopePtr xerces_scope, std::string const& nsxsd_map, std::string const& xsd_dir);
    virtual ~XMLEngine();
private:
	boost::scoped_ptr<xml::XMLTree> tree;
	boost::scoped_ptr<xml::XMLFragment> lsio;
	boost::scoped_ptr<xml::XMLSubtree> subtree;
};

enum response_code_e {
	XCAP_OK = 200,
	XCAP_OK_CREATED = 201,
	XCAP_FAIL_BAD_REQUEST = 400, ///< invalid HTTP request
	XCAP_FAIL_AUTHORIZATION = 401,///< unauthorized
	XCAP_FAIL_NOT_FOUND = 404,///< Not Found
	XCAP_FAIL_NOT_ALLOWED = 405,///< Method Not Allowed
	XCAP_FAIL_CONSTRAINTS = 409, /// <Conflict
	XCAP_FAIL_IF_PERFORM = 412,///< Precondition Failed
	XCAP_FAIL_MIME = 415,///< Unsupported Media Type
	XCAP_ERROR_INTERNAL = 500 ///< Internal Server Error
};

class XMLMethods : public XMLEngine {
    u8vector_t const default_ns;
    boost::shared_ptr<bmu::LexerChars> predicates;

    xercesc::DOMNode* put_create(
		xml::XMLSelect const& rparent
        , xml::nodestep_t const& nodexpath
        , xml::nsbindings_t const& prefixes
        , docsubtree_ptr const& docpart
    ) const;

    explicit XMLMethods(void) = delete; //NE

    u8vector_t percent_encoded(std::vector<xml::nodestep_t> const& vnodexpath, size_t len) const;

    void no_parent_ancestor(
        u8vector_t& docresponse
        , doctree_ptr xr_doc
        , std::vector<xml::nodestep_t> const& vnodexpath
        , size_t nsteps
        , xml::nsbindings_t const& prefixes
    ) const;

    u8vector_t fragment_invalid_error(void) const;

public:
    u8vector_t const& default_namespace(void) const { return default_ns; }

    //svi u prvom elementu vracaju odziv
	response_code_e get_xpath(
        u8vector_t& docpart
        , std::string& mimetype
        , u8vector_t const& docprev
        , std::vector<xml::nodestep_t> const& nodexpath
        , xml::nsbindings_t const& prefixes
    ) const;

	response_code_e put_xpath(
        u8vector_t& docnew
        , u8vector_t const& docprev
        , std::vector<xml::nodestep_t> const& nodexpath
        , xml::nsbindings_t const& prefixes
        , rawcontent_t const& docpart
        , std::string const& mimetype
    ) const;

    response_code_e del_xpath(
        u8vector_t& docnew
        , u8vector_t const& docprev
        , std::vector<xml::nodestep_t> const& nodexpath
        , xml::nsbindings_t const& prefixes
    ) const;

    XMLMethods(
		xml::XercesScopePtr xerces_scope
		, u8vector_t const& application_ns
        , std::string const& nsxsd_map
        , std::string const& xsd_dir
    );

    virtual ~XMLMethods();
};


}
}
