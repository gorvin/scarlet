#include "scarlet/xml/XMLBase.h"
#include "XMLUni.h"
#include "bmu/Logger.h"
#include <xercesc/internal/XMLScanner.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMDocumentFragment.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMLSSerializer.hpp>
#include <xercesc/dom/DOMLSOutput.hpp>
#include <xercesc/dom/DOMLSInput.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <boost/make_shared.hpp>
#include <map>

namespace scarlet {
namespace xml {

// set feature if the parser/document/serializer supports the feature/mode
#define SET_FEATURE(ps, fg, val) \
    if(ps->getDomConfig()->canSetParameter(fg, val)) { \
        ps->getDomConfig()->setParameter(fg, val);\
    } else {\
        DBGMSGAT("Feature: " << _TRLCP(fg) << " not supported!");\
    }

template <typename T>
void releaser(T* p) { p->release(); }

boost::mutex XercesScope::mutex;
XercesScope* XercesScope::one(0);

XercesScopePtr XercesScope::create(std::string const& locale)
{
	boost::mutex::scoped_lock lock(mutex);
	if (!one) {
		one = new XercesScope(locale);
		std::wclog << "Created new XercesScope instance " << std::endl;
	}
	else {
		std::wclog << "Using existing XercesScope instance " << std::endl;
	}
	BOOST_ASSERT(one);
	return XercesScopePtr(one)->shared_from_this(); // start reference counting then get second reference
}

XercesScopePtr XercesScope::instance(void)
{
	boost::mutex::scoped_lock lock(mutex);
	BOOST_ASSERT(one);
	return one->shared_from_this();
}

/** Inicijalizacija koristenih biblioteka (Xerces i Xalan). Interno se koristi na pocetku rada
    svih tipova koji koriste te biblioteke. */
XercesScope::XercesScope(std::string const& locale)
	: xl_locale()
{
	// don't lock mutex, it is already locked in create()
	BOOST_ASSERT(!one);
	xercesc::XMLPlatformUtils::Initialize(locale.c_str());
	if (xercesc::XMLPlatformUtils::fgTransService) {
		xl_locale = locale;
	}
	else {
		//cancel initialization and try with default locale
		std::locale global_locale(std::locale::global(std::locale("")));
		std::wclog << "Note: default system locale is: " << global_locale.name() << std::endl;
		xl_locale = global_locale.name();
		std::locale::global(global_locale);
		std::wclog << "Failed initialization for locale: " << locale << "\n\ttrying with default Xerces locale: "
			<< xercesc::XMLUni::fgXercescDefaultLocale << std::endl;
		xercesc::XMLPlatformUtils::Terminate();
		xercesc::XMLPlatformUtils::Initialize();
	}
	if (!xercesc::XMLPlatformUtils::fgTransService) {
		std::wclog << "ERROR: Failed initializing Xerces transcoding service, can't continue!!! " << std::endl;
		std::terminate();
	}
	std::wclog << "**** Initialized Xerces ****" << std::endl;
}

XercesScope::~XercesScope()
{
	boost::mutex::scoped_lock lock(mutex);
	one = nullptr; // reset one because this was last reference
	std::wclog << "**** Uninitializing Xerces ****" << std::endl;
	xercesc::XMLPlatformUtils::Terminate();
}

//vidljivo iz XMLUni.h
xml_engine_transcoder_t const* tr(void)
{
	BOOST_ASSERT(!XercesScope::instance()->getLocaleString().empty());
	//Na kraju svakog Boost.Thread se automatski zove on_thread_exit() iz boost/thread/detail/tss_hooks.hpp. 
	//Ona uklanja TLS vrijednost za taj thread koji se gasi. Ako bi koristi custom thread-ve onda
	//isto moras zvati tu funkcija na kraju threada
	static boost::thread_specific_ptr<xml_engine_transcoder_t> _tr;
	if(!_tr.get()) {
        _tr.reset(new xml_engine_transcoder_t);
        assert(_tr.get());
    }
    return _tr.get();
}

xercesc::DOMImplementationLS* domls(void)
{
	static xercesc::DOMImplementationLS* _domls_impl;
	if (!_domls_impl) {
		xmlstring lsname(_TRLCP("LS"));
		_domls_impl = xercesc::DOMImplementationRegistry::getDOMImplementation(lsname.c_str());
	}
	return _domls_impl;
}

/////////////////////////////////////////////////////////

bool is_valid_utf8(u8unit_t const* str, size_t length)
{
    xmlstring tmp;
    return _TRUTF8(tmp, str, length);
}

/////////////////////////////////////////////////////////

void xml_engine_exception_handler(void)
{
    try {
        throw;
    } catch(xercesc::SAXException const& ex) {
        std::wcerr << "SAXException: " << _TRLCP(ex.getMessage()) << std::endl;
    } catch(xercesc::XMLException const& ex) {
        std::wcerr << "XMLException: " << _TRLCP(ex.getMessage()) << std::endl;
    } catch(xercesc::OutOfMemoryException const& ex) {
        std::wcerr << "OutOfMemoryException: " << _TRLCP(ex.getMessage()) << std::endl;
    } catch(xercesc::DOMException const& ex) {
        std::wcerr << "DOMException: " << _TRLCP(ex.getMessage()) << std::endl;
    } catch(std::exception const& ex) {
        std::wcerr << "std::exception: " << ex.what() << std::endl;
    } catch(std::string const& ex) {
        std::wcerr << "Info exception string: " << ex << std::endl;
    } catch(char const* const& ex) {
        std::wcerr << "Info exception cstring: " << ex << std::endl;
    } catch(...) {
        std::wcerr << "Unknown Exception " << __FUNCTION__ << std::endl;
    }
}

///////////////////////////////////////////////////////////

EntityLocator::EntityLocator(std::string const& xsd_dir)
 : xsd_dir(xsd_dir)
{
    std::wclog << "Schema path: " << xsd_dir.c_str() << " ... ";
    if(!boost::filesystem::exists(xsd_dir)) {
        std::wclog << "Not found" << std::endl;
        assert(0); //std::terminate();
    }
    std::wclog << "OK" << std::endl;
}

xercesc::InputSource* EntityLocator::resolveEntity(const XMLCh* const /*publicId*/
                                                   , const XMLCh* const systemId)
{
    if(systemId) {
        std::string const schema_path((boost::filesystem::path(xsd_dir) / bmu::utf8_string(_TRLCP(systemId))).string());
        std::wclog << "Schema file/entity: " << schema_path.c_str() << std::endl;
        if(boost::filesystem::exists(schema_path))
            return new xercesc::LocalFileInputSource(_TRLCP(schema_path.c_str()).c_str());
    }
    return 0;
}

////////////////////////////////////////////////////////////

void ErrReporterBase::report(xercesc::SAXParseException const& ex
                     , char const* prefix)
{
    std::wclog << prefix << _TRLCP(ex.getMessage())
              << " at line: " << ex.getLineNumber() << std::endl;
}

void ErrReporterBase::error(unsigned int const errCode
                   , XMLCh const* const msgDomain
                   , xercesc::XMLErrorReporter::ErrTypes const errType
                   , XMLCh const* const errorText
                   , XMLCh const* const systemId
                   , XMLCh const* const publicId
                   , XMLFileLoc const lineNum
                   , XMLFileLoc const colNum)
{
    xercesc::SAXParseException toThrow(errorText
                                       , publicId
                                       , systemId
                                       , lineNum
                                       , colNum
                                       , getMemoryManager());

    switch(errType) {
    case xercesc::XMLErrorReporter::ErrType_Warning:
        ++wcount;
        DBGMSGAT("Warning: ");
        messages.push_back("Warning");
        break;
    case xercesc::XMLErrorReporter::ErrType_Error:
        ++ecount;
        DBGMSGAT("Error: ");
        messages.push_back("Error");
        break;
    default:
        ++ecount;
        DBGMSGAT("Fatal error: ");
        messages.push_back("Fatal");
    }

    messages.back().append(" at line ").append(bmu::to_string(lineNum, std::dec))
                   .append(" and column ").append(bmu::to_string(colNum, std::dec))
                   .append(": ").append(bmu::utf8_string(_TRLCP(errorText)));

    DBGMSG("\twith code " << errCode << " from domain " << _TRLCP(msgDomain)
            << " at line " << lineNum << ",\n\tError message: " << bmu::utf8_string(_TRLCP(errorText)));

    //NOTE: Za fatalne greske se podrazumijeva prekid skeniranja. Pored toga za fatalne greske
    //iz domena xercesc::XMLUni::fgExceptDomain se prekida bacanjem SAXParserException.
    //Za manje vazne greske se nastavlja ali ako vidis da ipak treba prekinuti mozes ovdje baciti
    //toThrow i postaviti odgovarajuci status u fstatus

    //NOTE: not-schema-valid nije eksplicitno fatalna greska kao not-well-formed ali se u skeneru
    // tretira kao fatalna ako je XercesDOMParser::setValidationConstraintFatal(true);
    if(msgDomain == xercesc::XMLUni::fgValidityDomain) {//tretira se kao fatalna
        fstatus = XML_NOT_SCHEMA_VALID;
		DBGMSGAT("*** It's validation error");
    } else if(errType > xercesc::XMLErrorReporter::ErrType_Error) { //uvijek fatalna
        //ostalo tretiram kao not-well-formed
        fstatus = XML_NOT_WELL_FORMED;
        if(msgDomain == xercesc::XMLUni::fgXMLErrDomain) {
            DBGMSGAT("*** It's well-formedness error");
        } else {
            DBGMSGAT("*** It's other fatal error, assuming well-formedness error");
        }
    } else { // errType <= xercesc::XMLErrorReporter::ErrType_Error)
        //nastavlja parsiranje ako se ne baci izuzetak //fstatus = XML_NOT_WELL_FORMED;
        DBGMSGAT("*** Still well-formed and schema valid but with warnings or other non fatal errors");
    }
}

///////////////////////////////////////////////////////////////

XMLTree::XMLTree(XercesScopePtr xersces_scope, std::string const& nsxsdmap, std::string const& xsd_dir)
 : xersces_scope(xersces_scope)
 , ErrReporterBase()
 , xercesc::XercesDOMParser()
 , entities(xsd_dir)
{
    ///XercesDOMParser::useScanner(transcoded<XMLCh>("SGXMLScanner").c_str());

    XercesDOMParser::setDoNamespaces(true);//Enable namespaces

    //enable checking of all Schema constraints
    XercesDOMParser::setValidationScheme(xercesc::XercesDOMParser::Val_Always);//enable schema validation
    XercesDOMParser::setDoSchema(true);
    // particle unique attribution constraint checking and particle derivation restriction checking
    XercesDOMParser::setValidationSchemaFullChecking(true);
    XercesDOMParser::setLoadSchema(true);
    XercesDOMParser::cacheGrammarFromParse(true);
    XercesDOMParser::setValidationConstraintFatal(true);

    //XercesDOMParser::setErrorHandler(&errors);// when detects violations of the schema.
    XercesDOMParser::getScanner()->setErrorReporter(this);
    XercesDOMParser::setEntityResolver(&entities);//find the schema and resolve schema imports/includes.

    std::wclog << "NS xsd map " << (nsxsdmap.empty() ? "is" : "not") << " empty" << std::endl;

    xmlstring schemaLocations = _TRLCP(nsxsdmap.c_str());
    XercesDOMParser::setExternalSchemaLocation(schemaLocations.c_str());

    std::wclog << "Created XML parser." << std::endl;
}


//void XMLTree::schema_validation(bool enable)
//{
//    if(enable) {
//        XercesDOMParser::setValidationScheme(xercesc::XercesDOMParser::Val_Always);//enable schema validation
//        XercesDOMParser::setDoSchema(true);
//        // particle unique attribution constraint checking and particle derivation restriction checking
//        XercesDOMParser::setValidationSchemaFullChecking(true);
//        XercesDOMParser::setLoadSchema(true);
//        XercesDOMParser::cacheGrammarFromParse(true);
//        XercesDOMParser::setValidationConstraintFatal(true);
//    } else {
//        XercesDOMParser::setValidationScheme(xercesc::XercesDOMParser::Val_Never);//disable schema validation
//        XercesDOMParser::setDoSchema(false);
//        XercesDOMParser::setValidationSchemaFullChecking(false);
//        XercesDOMParser::setLoadSchema(false);
//        XercesDOMParser::cacheGrammarFromParse(false);
//        XercesDOMParser::setValidationConstraintFatal(false);
//    }
//}


/* All XML processors MUST be able to read entities in both the UTF-8 and UTF-16 encodings.

Entities encoded in UTF-16 MUST and entities encoded in UTF-8 MAY begin with the Byte Order Mark(BOM)
described by Annex H of [ISO/IEC 10646:2000], section 16.8 of [Unicode] (the ZERO WIDTH NO-BREAK
SPACE character, #xFEFF). This is an encoding signature, not part of either the markup or the
character data of the XML document. XML processors MUST be able to use this character to
differentiate between UTF-8 and UTF-16 encoded documents.

If the replacement text of an external entity is to begin with the character U+FEFF, and no text
declaration is present, then a Byte Order Mark MUST be present, whether the entity is encoded in
UTF-8 or UTF-16.

Although an XML processor is required to read only entities in the UTF-8 and UTF-16 encodings, it is
recognized that other encodings are used around the world, and it may be desired for XML processors
to read entities that use them. In the absence of external character encoding information (such as
MIME headers), parsed entities which are stored in an encoding other than UTF-8 or UTF-16 MUST begin
with a text declaration (see 4.3.1 The Text Declaration) containing an encoding declaration:
[80] EncodingDecl ::= S 'encoding' Eq ('"' EncName '"' | "'" EncName "'" )
[81] EncName	  ::= [A-Za-z] ([A-Za-z0-9._] | '-')*  //Encoding name contains only Latin characters
*/

//sa forsiranjem UTF-8: src.setEncoding(xercesc::XMLUni::fgUTF8EncodingString);
//javice se izuzetak ako nije UTF-8 pa bi trebao prije prepoznati zbog cega je izuzetak
//CHECK: Vidi ima li nefatalnih gresaka koje su prihvatljive prema RFC 4825
//ovdje se sve greske osim XML_NOT_UTF8 tretiraju kao XML_NOT_WELL_FORMED
// xr_doc->getXmlEncoding() je postavljeno samo ako postoji u xml deklaraciji
// xr_doc->getInputEncoding() je uvijek postavljen ako je uspjelo parsiranje
// xr_doc vec je normalizovan u parse(...), ne treba xr_doc->normalize();
doctree_ptr XMLTree::parse(u8unit_t const* xml_str, size_t xml_str_size)
{
	try {
		if(xml_str && xml_str_size>0) {
			ErrReporterBase::clear_messages();
			xercesc::MemBufInputSource src((XMLByte const*)xml_str, xml_str_size, "tmp_doc_istream");
			XercesDOMParser::parse(src);
			DBGMSGAT("Parsed with "
					 << ErrReporterBase::nwarnings() << " warnings, "
					 << ErrReporterBase::nerrors() << " errors and validity "
					 << ErrReporterBase::validity());
			if(ErrReporterBase::nerrors() == 0) {
				xercesc::DOMDocument* xr_doc(XercesDOMParser::adoptDocument());
				assert(ErrReporterBase::validity() == XML_VALID);
				if(xr_doc) {
					DBGMSGAT("Actual input encoding: " << _TRLCP(xr_doc->getInputEncoding()));
					static xmlstring const EncNameUTF8(xercesc::XMLUni::fgUTF8EncodingString);
					if(EncNameUTF8 == xr_doc->getInputEncoding()) {
						return doctree_ptr(xr_doc, &releaser<xercesc::DOMDocument>);
					} 
					else {
						ErrReporterBase::validity(XML_NOT_UTF8);
					}
				} else {
					DBGMSGAT("Failed adopting parsed DOMDocument");
				}
			}
		}
	} 
	catch (...) {
		xml_engine_exception_handler();
	}
    DBGMSGAT("Failed parsing:\n" << std::string((char const*)xml_str, xml_str_size));
    if(ErrReporterBase::validity() == XML_VALID)
        ErrReporterBase::validity(XML_NOT_WELL_FORMED);
    return doctree_ptr();
}


///////////////////////////////////////////////////


XMLSubtree::XMLSubtree(XercesScopePtr xersces_scope)
 : xersces_scope(xersces_scope)
 , ErrReporterBase()
 , DOMLSParserImpl()
 , target_input(domls()->createLSInput(), &releaser<xercesc::DOMLSInput>)
{
    assert(target_input.get());

    ///DOMLSParserImpl::useScanner(transcoded<XMLCh>("WFXMLScanner").c_str());

    DOMLSParserImpl::setDoNamespaces(true);//Enable namespaces
    DOMLSParserImpl::getScanner()->setErrorReporter(this);

    DOMLSParserImpl::setValidationScheme(Val_Never);
    DOMLSParserImpl::setIncludeIgnorableWhitespace(true);

    SET_FEATURE(this, xercesc::XMLUni::fgXercesUserAdoptsDOMDocument, true);
    SET_FEATURE(this, xercesc::XMLUni::fgDOMElementContentWhitespace, true)

    std::wclog << "Created XML LS parser." << std::endl;
}


XMLSubtree::~XMLSubtree() { }


size_t XMLSubtree::nelements(xercesc::DOMNodeList const* nodelist)
{
    if(!nodelist) return 0;

    size_t count(0);
    for(size_t i=0; i<nodelist->getLength(); ++i) {
        if(nodelist->item(i)->getNodeType() == xercesc::DOMNode::ELEMENT_NODE)
            count ++;
    }
    return count;
}


xercesc::DOMNode* XMLSubtree::element_node(xercesc::DOMNodeList const* nodelist)
{
    for(size_t i=0; i<nodelist->getLength(); ++i) {
        xercesc::DOMNode* element(nodelist->item(i));
        if(element->getNodeType() == xercesc::DOMNode::ELEMENT_NODE)
            return element;
    }
    return 0;
}

docsubtree_ptr
 XMLSubtree::null_with_error(xml_validity_e eval)
{
    if(eval == XML_VALID) eval = XML_NOT_WELL_FORMED;
    ErrReporterBase::validity(eval);
    return docsubtree_ptr();
}


doctree_ptr
 XMLSubtree::parse_document(u8vector_t const& document)
{
    xercesc::MemBufInputSource src(reinterpret_cast<XMLByte const*>(&document[0])
                                   , document.size()
                                   , "tmp_doc_istream");

    target_input->setByteStream(&src);
    //target_input->setEncoding(xercesc::XMLUni::fgUTF8EncodingString);//CHECK:

    return doctree_ptr(DOMLSParserImpl::parse(target_input.get())
                                                   , &releaser<xercesc::DOMDocument>);
}


docsubtree_ptr
 XMLSubtree::parse(doctree_ptr const& xr_doc
                   , u8unit_t const* xml_str
                   , size_t xml_str_size
                   , u8vector_t const& default_ns)
{
    assert(xr_doc.get());

    if(!is_valid_utf8(xml_str, xml_str_size)) return null_with_error(XML_NOT_UTF8);

    //make sure we have one root element and use default namespace binding
    u8vector_t fragment((u8unit_t*)("<fragment xmlns=\""));
    fragment.append(default_ns).append((u8unit_t*)"\">")
            .append(xml_str, xml_str_size).append((u8unit_t*)"</fragment>");

    xercesc::MemBufInputSource src(reinterpret_cast<XMLByte const*>(&fragment[0])
                                   , fragment.size()
                                   , "tmp_doc_istream");

    target_input->setByteStream(&src);
    //target_input->setEncoding(xercesc::XMLUni::fgUTF8EncodingString);//CHECK:

    doctree_ptr frdoc;
    try {
        frdoc = doctree_ptr(DOMLSParserImpl::parse(target_input.get())
                                                        , &releaser<xercesc::DOMDocument>);
    } catch (...) {
        xml_engine_exception_handler();
        return null_with_error(ErrReporterBase::validity());
    }

    if(ErrReporterBase::validity() != XML_VALID || !frdoc.get())
        return null_with_error(ErrReporterBase::validity());

    static xmlstring const EncNameUTF8(xercesc::XMLUni::fgUTF8EncodingString);

    if(EncNameUTF8 != frdoc->getInputEncoding())
        return null_with_error(XML_NOT_UTF8);

    xercesc::DOMElement* const fr_root(frdoc->getDocumentElement());
    xercesc::DOMNodeList* const frnodes(fr_root ? fr_root->getChildNodes() : 0);

    //body has to be a well-balanced region of an XML document, including only a SINGLE element.
    if(!frnodes || nelements(frnodes) != 1)
        return null_with_error(XML_NOT_WELL_FORMED);

    docsubtree_ptr holder(
        xr_doc->createDocumentFragment()
        , &releaser<xercesc::DOMDocumentFragment>
    );

    assert(holder.get()); //TODO: baci out_of_memory

    //I whitespace cvorovi se prema RFC 4825 kopiraju iako mora biti samo jedan ELEMENT_NODE
    try {
        for(size_t i=0; i<frnodes->getLength(); ++i)
            holder->appendChild(xr_doc->importNode(frnodes->item(i), true));

        return holder;
    } catch (...) {
        xml_engine_exception_handler();
    }

    return null_with_error(XML_NOT_WELL_FORMED);
}


xercesc::DOMNode*
 XMLSubtree::put(xercesc::DOMNode* near_node
                 , docsubtree_ptr const& docpart
                 , put_node_context_t ctx) const
{
    assert(near_node);
    assert(docpart.get());
    assert(XMLSubtree::nelements(docpart->getChildNodes()) == 1);

    xercesc::DOMNode* const parent(near_node->getParentNode());
    xercesc::DOMNode* const element(XMLSubtree::element_node(docpart->getChildNodes()));//ELEMENT_NODE
    assert(element->getNodeType() == xercesc::DOMNode::ELEMENT_NODE);

    try {
        switch(ctx) {
        case PUT_BEFORE_NODE: parent->insertBefore(docpart.get(), near_node); break;
        case PUT_AFTER_NODE: parent->insertBefore(docpart.get(), near_node->getNextSibling()); break;
        case PUT_REPLACE_NODE: {
            parent->insertBefore(docpart.get(), near_node);
            parent->removeChild(near_node)->release();

        } break;
        case PUT_APPEND_NODE: near_node->appendChild(docpart.get()); break;
        }
    } catch(...) {
        xml_engine_exception_handler();
        return 0;
    }
    return element;
}


///////////////////////////////////////////////////


u8fmttarget_t::u8fmttarget_t(u8vector_t& str)
 : _str(str)
{
//    _str.clear();
    _str.reserve(BLOCK_SIZE);
}


void u8fmttarget_t::writeChars(const XMLByte* const toWrite
                               , const XMLSize_t      count
                               , xercesc::XMLFormatter* const /*formatter*/)
{
    size_t const full(_str.size() + count);
    if(full > _str.capacity())
        _str.reserve(_str.capacity() + ((size_t)full/BLOCK_SIZE)*BLOCK_SIZE);

    _str.append(toWrite, toWrite + count);
}


///////////////////////////////////////////////////


XMLFragment::XMLFragment(XercesScopePtr xersces_scope)
 : xersces_scope(xersces_scope)
 , serializer(domls()->createLSSerializer(), &releaser<xercesc::DOMLSSerializer>)
 , target_output(domls()->createLSOutput(), &releaser<xercesc::DOMLSOutput>)
{
    assert(serializer.get());
    assert(target_output.get());

    DBGMSGAT("Setting features on DOMLSSerializer");
    SET_FEATURE(serializer, xercesc::XMLUni::fgDOMWRTSplitCdataSections, false);
    SET_FEATURE(serializer, xercesc::XMLUni::fgDOMWRTDiscardDefaultContent, true);
    SET_FEATURE(serializer, xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, false);// turn off "pretty print"
    SET_FEATURE(serializer, xercesc::XMLUni::fgDOMWRTBOM, false);

    //Kad se ispisuje citav dokument zelim selektivno ispisivati xml deklaraciju a ne uvijek citavu
    //da bi xml deklaracija izlaznog dokumenta bila u skladu sa xml deklaracijom ulaznog.
    SET_FEATURE(serializer, xercesc::XMLUni::fgDOMXMLDeclaration, false);
}


XMLFragment::~XMLFragment() { }


void XMLFragment::serialize(xercesc::DOMNode const* node) const
{
    u8vector_t tmp;
    XMLFragment::serialize(tmp, node);
    std::cout << bmu::utf8_string(tmp) << std::endl;
}


//The server MUST NOT add namespace bindings representing namespaces used by the element
// or its children, but declared in ANCESTORS element
//1. konfiguracioni parametri za onemogucenje xmlns deklaracija ne funkcionisu
//2. DOMLSSerializerFilteru se ne prosledjuju atributi pa ni xmlns
//Zato kopiram podstablo pod cvor 'fragment' izmedju dva tekst cvora "\n", serializer ce
//tako xmlns direktivu pridruziti cvoru 'fragment' a podstablo (bez xmlns direktive) ce biti
//u privremenom stringu izmedju prvog i zadnjeg znaka '\n' sto na kraju kopiram u izlazni string.
bool XMLFragment::serialize(u8vector_t& xml_str, xercesc::DOMNode const* node) const
{
    assert(node);

    xml_str.clear();

    XMLCh const* const prev_encoding(target_output->getEncoding());
    target_output->setEncoding(xercesc::XMLUni::fgUTF8EncodingString);

    bool status(false);

    if(node->getNodeType() == xercesc::DOMNode::DOCUMENT_NODE) {
        DBGMSGAT("Writing document ");

        xercesc::DOMDocument const* docnode(static_cast<xercesc::DOMDocument const*>(node));
        //"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"

        xml_str.append((u8unit_t*)"<?xml");
        {
            XMLCh const* version(docnode->getXmlVersion());
            if(version) {
                u8vector_t tmp;
                _TRUTF8(tmp, version);
                if(!tmp.empty()) {
                    xml_str.append((u8unit_t*)" version=\"").append(tmp);
                    xml_str.push_back('\"');
                }
            }
        }
        {
            XMLCh const* encoding(docnode->getXmlEncoding());
            if(encoding) {
                u8vector_t tmp;
                _TRUTF8(tmp, encoding);
                if(!tmp.empty()) {
                    xml_str.append((u8unit_t*)" encoding=\"").append(tmp);
                    xml_str.push_back('\"');
                }
            }
        }

        if(docnode->getXmlStandalone()) {
            xml_str.append((u8unit_t*)" standalone=\"yes\"");
        }

        xml_str.append((u8unit_t*)"?>\n");

        u8fmttarget_t targetu8str(xml_str);
        target_output->setByteStream(&targetu8str);

        try{
            status = serializer->write(node, target_output.get());
        } catch(...) {
            xml_engine_exception_handler();
        }

    } else if(node == (xercesc::DOMNode const*)node->getOwnerDocument()->getDocumentElement()) {//root node
        DBGMSGAT("Writing root element");

        //root nema ELEMENT_NODE ancestora pa ni njihovih xmlns direktiva
        //tako da ne treba workaround sa fragment elementom

        u8fmttarget_t targetu8str(xml_str);
        target_output->setByteStream(&targetu8str);

        try{
            status = serializer->write(node, target_output.get());
        } catch(...) {
            xml_engine_exception_handler();
        }
    } else {
        DBGMSGAT("Writing subtree element");
        xercesc::DOMDocument* xdoc(node->getOwnerDocument());
        assert(xdoc);
        docsubtree_ptr holder(
            xdoc->createDocumentFragment()
            , &releaser<xercesc::DOMDocumentFragment>
        );
        assert(holder.get()); //TODO: baci out_of_memory

        holder->appendChild(xdoc->createElementNS(node->getNamespaceURI(),_TRLCP("fragment").c_str()));
        xercesc::DOMNode* const fr_node(holder->getFirstChild());
        fr_node->appendChild((xercesc::DOMNode*)xdoc->createTextNode(_TRLCP("\n").c_str()));
        fr_node->appendChild(node->cloneNode(true));
        fr_node->appendChild((xercesc::DOMNode*)xdoc->createTextNode(_TRLCP("\n").c_str()));

        u8vector_t fragment_str;
        u8fmttarget_t targetu8str(fragment_str);
        target_output->setByteStream(&targetu8str);//switch to string target

        try{
            status = serializer->write(fr_node, target_output.get());
            xml_str.assign(++std::find(fragment_str.begin(), fragment_str.end(), '\n')
                           , (++std::find(fragment_str.rbegin(), fragment_str.rend(), '\n')).base());
        } catch(...) {
            xml_engine_exception_handler();
        }
    }

    target_output->setEncoding(prev_encoding);
    return status;
}


}
}
