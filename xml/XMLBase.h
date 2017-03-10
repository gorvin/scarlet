#include <bmu/tydefs.h>
#include <scarlet/xml/xmldefs.h>
#include <boost/filesystem/operations.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/tss.hpp>
#include <xercesc/sax/EntityResolver.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/parsers/DOMLSParserImpl.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace XERCES_CPP_NAMESPACE {
    class DOMNode;
    class DOMDocument;
    class DOMLSSerializer;
    class XMLFormatTarget;
    class DOMLSOutput;
    class DOMLSParser;
    class DOMLSInput;
    class DOMDocumentFragment;
    class DOMImplementationLS;
}

namespace scarlet {
namespace xml {
using ::bmu::u8unit_t;
using ::bmu::u8vector_t;
using ::bmu::u8vector_it;

enum xml_validity_e {
	XML_VALID,
	XML_NOT_WELL_FORMED,
	XML_NOT_SCHEMA_VALID,
	XML_NOT_UTF8
};

enum put_node_context_t {
	PUT_BEFORE_NODE,
	PUT_AFTER_NODE,
	PUT_REPLACE_NODE,
	PUT_APPEND_NODE
};

typedef boost::shared_ptr<xercesc::DOMDocument> doctree_ptr;
typedef boost::shared_ptr<xercesc::DOMDocumentFragment> docsubtree_ptr;

class XercesScope : public boost::enable_shared_from_this<XercesScope>, public boost::noncopyable {
	friend class XMLScope;
	XercesScope(void) = delete;
	// initializes xerces
	XercesScope(std::string const& locale);
public:
	static XercesScopePtr create(std::string const& locale);
	static XercesScopePtr instance(void);
	// uninitializes xerces
	~XercesScope();
	std::string getLocaleString(void) { return xl_locale;  }
private:
	static boost::mutex       mutex;
	static XercesScope*       one; //boost::make_shared<XercesScope>(locale)
	std::string               xl_locale;
};

bool is_valid_utf8(u8unit_t const* str, size_t length);

class EntityLocator : public xercesc::EntityResolver {
    std::string const xsd_dir;
    explicit EntityLocator(void); //NE
    xercesc::InputSource* resolveEntity (const XMLCh* const publicId
                                         , const XMLCh* const systemId);
public:
    EntityLocator(std::string const& xsd_dir);
};

class ErrReporterBase {
    size_t wcount;
    size_t ecount;
    xml_validity_e fstatus;
    std::vector<std::string> messages;
protected:
    ErrReporterBase(void)
     : wcount(0)
     , ecount(0)
     , fstatus(XML_VALID)
     { }
    virtual ~ErrReporterBase() { }
    virtual xercesc::MemoryManager* getMemoryManager() const = 0;
    void report(xercesc::SAXParseException const& ex
                          , char const* prefix);
    void resetErrors(void) { wcount = ecount = 0; fstatus = XML_VALID; }
    void error(unsigned int const errCode
               , XMLCh const* const msgDomain
               , xercesc::XMLErrorReporter::ErrTypes const errType
               , XMLCh const* const errorText
               , XMLCh const* const systemId
               , XMLCh const* const publicId
               , XMLFileLoc const lineNum
               , XMLFileLoc const colNum);
    void validity(xml_validity_e fvalidity) { fstatus = fvalidity; }
    void clear_messages(void) { messages.clear(); }
public:
    xml_validity_e validity(void) const { return fstatus; }
    size_t nwarnings(void) const { return wcount; }
    size_t nerrors(void) const { return ecount; }
    std::vector<std::string> const& get_messages(void) const { return messages; }
};


/** Provjera ispravnosti dokumenata i formiranje DOM stabla. */
class XMLTree 
	: public ErrReporterBase
    , private xercesc::XercesDOMParser {
    explicit XMLTree(void) = delete; //NE
    xercesc::MemoryManager* getMemoryManager() const { return XercesDOMParser::getMemoryManager(); }
    void resetErrors(void) { ErrReporterBase::resetErrors(); }
    void error(unsigned int const errCode
               , XMLCh const* const msgDomain
               , xercesc::XMLErrorReporter::ErrTypes const errType
               , XMLCh const* const errorText
               , XMLCh const* const systemId
               , XMLCh const* const publicId
               , XMLFileLoc const lineNum
               , XMLFileLoc const colNum)
    {
        ErrReporterBase::error(errCode, msgDomain, errType, errorText
                               , systemId, publicId, lineNum, colNum);
    }
public:
    void* operator new(size_t size) { return XercesDOMParser::operator new(size); }
    void operator delete(void* p) { return XercesDOMParser::operator delete(p); }
    //void schema_validation(bool enable = true);
    /** \param nsxsdmap Uparuje namespace URIje sa njihovim semama, ovako treba da izgleda:
     <code>
     char const* nsxsdmap =
      "http://example.com                     test_schema.xsd       "
      "http://www.w3.org/XML/1998/namespace   xml.xsd               ";
     </code>
     Ti *.xsd fajlovi se trebaju nalaziti u 'base/subxsd/'
     \param base Putanja pod kojom su svi konfiguracioni resursi servera.
     \param subxsd Podputanja od \ref base pod kojom su sve XML seme servera.
    */
	XMLTree(XercesScopePtr xersces_scope, std ::string const& nsxsdmap, std::string const& xsd_dir);
    /** U rezultatu daje pokazivac DOM stabla zadanog dokumenta. Pokazivac je 0 u slucaju greske.
        S DOM stablom nemas sta direktno raditi, koristitis ga samo kao argument XmlXpath::query.
     */
    doctree_ptr parse(u8unit_t const* xml_str, size_t xml_str_size);
private:
	XercesScopePtr xersces_scope;
    EntityLocator  entities;
};

class XMLSubtree
	: public ErrReporterBase
    , private xercesc::DOMLSParserImpl {
	XMLSubtree(void) = delete;
    xercesc::MemoryManager* getMemoryManager() const { return DOMLSParserImpl::getMemoryManager(); }
    void resetErrors(void) { ErrReporterBase::resetErrors(); }
    void error(unsigned int const errCode
               , XMLCh const* const msgDomain
               , xercesc::XMLErrorReporter::ErrTypes const errType
               , XMLCh const* const errorText
               , XMLCh const* const systemId
               , XMLCh const* const publicId
               , XMLFileLoc const lineNum
               , XMLFileLoc const colNum)
    {
        ErrReporterBase::error(errCode, msgDomain, errType, errorText
                               , systemId, publicId, lineNum, colNum);
    }
    static size_t nelements(xercesc::DOMNodeList const* nodelist);
    static xercesc::DOMNode* element_node(xercesc::DOMNodeList const* nodelist);
    docsubtree_ptr null_with_error(xml_validity_e eval);
public:
    void* operator new(size_t size) { return DOMLSParserImpl::operator new(size); }
    void operator delete(void* p) { return DOMLSParserImpl::operator delete(p); }
    XMLSubtree(XercesScopePtr xersces_scope);
    ~XMLSubtree(void);
    docsubtree_ptr parse(
        doctree_ptr const& xr_doc
        , u8unit_t const* xml_str
        , size_t xml_str_size
        , u8vector_t const& default_ns
    );
    xercesc::DOMNode* put(
        xercesc::DOMNode* near_node
        , docsubtree_ptr const& docpart
        , put_node_context_t ctx
    ) const;
    doctree_ptr parse_document(u8vector_t const& document);
private:
	XercesScopePtr                         xersces_scope;
	boost::shared_ptr<xercesc::DOMLSInput> target_input;
};

class u8fmttarget_t : public xercesc::XMLFormatTarget {
    u8vector_t& _str; //u ovo se pise
    enum { BLOCK_SIZE = 1024 };
    explicit u8fmttarget_t(const u8fmttarget_t&); //NE
    u8fmttarget_t& operator=(const u8fmttarget_t&); //NE
    void writeChars(const XMLByte* const toWrite
                    , const XMLSize_t      count
                    , xercesc::XMLFormatter* const /*formatter*/);
public:
    u8fmttarget_t(u8vector_t& str);
};

/** Pretvaranje DOM podstabla u XML fragment:
  - formiranje XML fragmenta od nekog cvora iz DOM stabla (fragment serializer)
  - formiranje citavog dokumenta od korijena DOM stabla (document serializer)
  */
class XMLFragment {
    XercesScopePtr                              xersces_scope;
    boost::shared_ptr<xercesc::DOMLSSerializer> serializer;
    boost::shared_ptr<xercesc::DOMLSOutput>     target_output;
public:
    void serialize(xercesc::DOMNode const* node) const;
    bool serialize(u8vector_t& xml_str, xercesc::DOMNode const* node) const;
    XMLFragment(void) = delete;
	XMLFragment(XercesScopePtr xersces_scope);
	~XMLFragment();
};

}
}