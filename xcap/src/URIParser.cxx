#include "scarlet/xcap/URIParser.h"
#include "bmu/LexerChars.h"
#include "bmu/codepoint_iterator.hxx"
#include "bmu/Logger.h"
//#include "bmu/Options.h"
#include <boost/token_iterator.hpp>

namespace scarlet {
namespace xcap {
using ::bmu::u32char_t;

URIParser::URIParser(std::string const& default_domain)
 : predicates(new bmu::LexerChars)
 , default_domain(default_domain)
{
    assert(predicates.get());
}


/*
step ::= NameorAny |
         NameorAny "[" position "]" |
         NameorAny "[" attr-test "]" |
         NameorAny "[" position "]" "[" attr-test "]"
NameorAny          ::= QName | "*"   ; QName from XML Namespaces
position           ::= 1*DIGIT
attr-test          ::= "@" QName "=" AttValue ; AttValue is from XML specification
QName  ::= NCName ':' NCName | NCName
NCName   ::= NCNameStartChar (NCNameChar)*

Treba samo utvrditi da li je valjan step xcap node-selectora prema RFC 4825 ili prema
nekoj poznatoj ekstenziji. Podrazumijeva se da su sve moguce ekstenzije obuhvacen Xalanovom XPath
implementacijom tako da se i u slucaju poznate ekstenzije postupci GET/PUT/DELETE
obavljaju na identican nacin koristenjem Xalana.
Ako je zadnji step (target-selector) onda prije provjeris da li je "@" QName | "namespace::*" pa
tek onda trebas zvati is_valid_step(...)

  prefix: name [pos/att][att/null]
*/
bool URIParser::is_valid_step(u8vector_it itbeg, u8vector_it const itend, xml::nodestep_t& nsel) const
{
    DBGMSGAT("Validating step: " << std::string(itbeg, itend));

    bmu::codepoint_iterator<u8vector_it> it(itbeg, itbeg, itend);
    bmu::codepoint_iterator<u8vector_it> const end(itend, itbeg, itend);

    //NOTE: *it za it == end daje INVALID_CODEPOINT
    u8vector_it prev(itbeg);

    //QName | '*'
    if(*it == '*') {
        DBGMSGAT("Xpath step OK - wildcard NameorAny ::= '*'");
        nsel.wildcarded = true;
        nsel.localname = (u8unit_t*)"*";
        ++it;
    } else {
        if(!predicates->is_NCNameStartChar(*it)) {
            DBGMSGAT("Invalid xpath step - no at least NameorAny ::= QName ::= NCName, rest is: "
                     << std::string(it.base(), end.base()));
            return false;
        }
        it = std::find_if(it, end, predicates->not_NCNameChar);
        if(*it == ':') { //QName ::= NCName ':' NCName
            nsel.prefix.assign(prev, it.base());
            if(!predicates->is_NCNameStartChar(*++it)) {
                DBGMSGAT("Invalid xpath step - not found expected NameorAny ::= QName ::= NCName ':' NCName, rest is: "
                         << std::string(it.base(), end.base()));
                return false;
            }
            prev = it.base();
        }
        it = std::find_if(it, end, predicates->not_NCNameChar);
        nsel.localname.assign(prev, it.base());
    }

    //NOTE: i za ovaj slucaj trebas postaviti position
    if(it == end) {
        DBGMSGAT("Xpath step OK - simple step: '*' or QName");
        return true; //OK step '*' or QName
    }

    if(*it != '[') {
        DBGMSGAT("Invalid xpath step - no predicates, rest is: "
                 << std::string(it.base(), end.base()));
        return false;
    }

    u8vector_it const predicate1_start(++u8vector_it(it.base()));//ne mijenja itbeg

    it = std::find_if(it, end, bmu::match_closing<u32char_t, '[', ']'>());
    if(it == end) {
        DBGMSGAT("Invalid xpath step - no 1st predicate end");
        return false;
    }
    u8vector_it const predicate1_end(it.base());

    if(++it == end) { //ako je 1 predikat onda je pozicioni ILI atributski
        if(predicates->is_valid_pospredicate(predicate1_start, predicate1_end)) {
            bool ok = bmu::from_string(nsel.position
                                  , std::string(predicate1_start, predicate1_end)
                                  , std::dec);
            assert(ok);
            DBGMSGAT("Xpath OK - with one positional predicate");
            assert(nsel.position > 0); //baci zero_position_not_allowed
            return true;
        }
        DBGMSGAT("The only one predicate not positional, checking if it's attributive ...");
        return predicates->is_valid_attpredicate(predicate1_start
                                                 , predicate1_end
                                                 , nsel.attprefix
                                                 , nsel.attname
                                                 , nsel.attvalue);
    }
    //inace ce ocekuje i drugi predikat

    if(*it != '[') {
        DBGMSGAT("Invalid xpath step - no expected 2nd predicate, rest is "
                 << std::string(it.base(), end.base()));
        return false;
    }

    u8vector_it const predicate2_start(++u8vector_it(it.base()));
    it = std::find_if(it, end, bmu::match_closing<u32char_t, '[', ']'>());
    if(it == end) {
        DBGMSGAT("Invalid xpath step - no 2nd predicate end");
        return false;
    }

    u8vector_it const predicate2_end(it.base());

    if(++it != end) {
        DBGMSGAT("Invalid xpath step - there's more characters after 2nd predicate, rest is: "
                 << std::string(it.base(), end.base()));
        return false;//ima visak karaktera
    }

    //2 je predikata pa prvi mora biti pozicioni a drugi atributski

    if(!predicates->is_valid_pospredicate(predicate1_start, predicate1_end) ||
       !predicates->is_valid_attpredicate(predicate2_start
                                          , predicate2_end
                                          , nsel.attprefix
                                          , nsel.attname
                                          , nsel.attvalue)) {
        DBGMSGAT("Invalid xpath step - 1st predicate not positional or 2nd not attributive");
        return false;
    }

    bool ok = bmu::from_string(nsel.position
                          , std::string(predicate1_start, predicate1_end)
                          , std::dec);
    if(!ok) {
        std::wclog << "Unexpected digit encountered in already validated positional "
                     "predicate of node selector step." << std::endl;
        return false;
    }
    DBGMSGAT("Xpath OK - with two predicates");

    return nsel.position > 0;
}


bool URIParser::is_attselector(u8vector_it itbeg, u8vector_it const itend, xml::nodestep_t& nsel) const
{
    bmu::codepoint_iterator<u8vector_it> it(itbeg, itbeg, itend);
    bmu::codepoint_iterator<u8vector_it> const end(itend, itbeg, itend);

    //NOTE: *it za it == end daje INVALID_CODEPOINT

    if(*it != '@') {
        DBGMSGAT("Invalid attribute selector - not attribute, rest is "
                  << std::string(it.base(), end.base()));
        return false;
    }
    if(!predicates->is_NCNameStartChar(*++it)) {
        DBGMSGAT("Invalid attribute selector - no at least QName ::= NCName, rest is "
                 << std::string(it.base(), end.base()));
        return false; //nema QName
    }
    u8vector_it prev(it.base());
    it = std::find_if(it, end, predicates->not_NCNameChar);

    if(*it == ':') { //QName ::= NCName ':' NCName
        nsel.prefix.assign(prev, it.base());
        if(!predicates->is_NCNameStartChar(*++it)) {
            DBGMSGAT("Invalid attribute selector - no expected QName ::= NCName ':' NCName, rest is "
                     << std::string(it.base(), end.base()));
            return false;
        }
        prev = it.base();
        it = std::find_if(it, end, predicates->not_NCNameChar);
    }
    nsel.localname.assign(prev, it.base());
    return it == end;
}


bool URIParser::is_nsselector(u8vector_it itbeg, u8vector_it const itend) const
{
    static u32char_t const nssel[] = { 'n', 'a', 'm', 'e', 's', 'p', 'a', 'c', 'e', ':', ':', '*' };
    static std::ptrdiff_t const count(sizeof(nssel)/sizeof(u32char_t));

    bmu::codepoint_iterator<u8vector_it> it(itbeg, itbeg, itend);
    bmu::codepoint_iterator<u8vector_it> const end(itend, itbeg, itend);

    //NOTE: *it za it == end daje INVALID_CODEPOINT
    if(std::distance(itbeg, itend) != count) {
        DBGMSGAT("Invalid namespace selector - not same length as namespace::*");
        return false;
    }

    for(u32char_t const* p=nssel; it != end; ++p, ++it) {
        if(*p != *it) {
            DBGMSGAT("Invalid namespace selector - not equal to namespace::*");
            return false;
        }
    }
    DBGMSGAT("Namespace selector OK - equal to namespace::*");
    return true;
}


//NOTE: za Xerces i Xalan moras pripremiti UTF-8, ne smijes direktno UTF-16 (xmlstring) jer
//je XMLCh u stvari wchar_t a on nije uvijek 2 bajta npr. Solaris, neki Linuxi, Mac koriste
//wchar_t od 4 bajta. Zatim neka sebi prekoduju iz UTF-8 u odgovarajuci UTF-16 format.

//NOTE: If the URI includes unknown extension-selectors reject the request with a 404 (Not Found).
void URIParser::validated(std::vector<xml::nodestep_t>& nsel, u8vector_it itbeg, u8vector_it const itend) const
{
    if(itbeg == itend) {
        DBGMSGAT("Empty xpath");
        nsel.clear();
        return;
    }

    nsel.clear();

    //u8vector_it const node_selector_start(itbeg);

    bmu::codepoint_iterator<u8vector_it> it(itbeg, itbeg, itend);
    bmu::codepoint_iterator<u8vector_it> const end(itend, itbeg, itend);

    //NOTE: *it za it == end daje INVALID_CODEPOINT

    //radi jednostavnosti podrazumijeva se '/' na pocetku iako nije u sklopu node-selectora
    assert(*it == '/');

    u8vector_it prev((++it).base());
    it = std::find(it, end, (u32char_t)'/');

    while(it != end) {//until last step

        nsel.resize(nsel.size()+1);

        if(!is_valid_step(prev, it.base(), nsel.back())) {

            DBGMSGAT("Invalid xpath step: " << std::string(prev, it.base()));
            nsel.clear();
            return;

        }
        nsel.back().steppath.assign(prev, it.base());
        prev = (++it).base();
        it = std::find(it, end, (u32char_t)'/');

    }

    nsel.resize(nsel.size()+1);
    if(is_attselector(prev, itend, nsel.back())) {
        nsel.back().type = xml::nodestep_t::NODE_ATTRIBUTE;
    } else if(is_nsselector(prev, itend)) {
        nsel.back().type = xml::nodestep_t::NODE_NAMESPACE;
    } else if(!is_valid_step(prev, itend, nsel.back())) {
        DBGMSGAT("Invalid xpath target: " << std::string(prev, itend));
        nsel.clear();
    }
    if(!nsel.empty())
        nsel.back().steppath.assign(++prev, itend);
}


/* namespace bindings in query use this grammar (XPointer Framework xmlns() scheme)

( S* xmlns S* '(' S* XmlnsSchemeData S* ')' )*

xmlns ::= "xmlns"

XmlnsSchemeData ::= NCName S* '=' S* EscapedSchemeData*

S               ::= ' ' | '\t' | 0x0D | 0x0A

EscapedSchemeData ::= NormalChar | EscapedChar | '(' EscapedSchemeData* ')'
EscapedChar       ::= "^(" | "^)" | "^^"
NormalChar        ::= [0x0-0xFF] - '(' - ')' - '^'

 The end of a pointer part is signaled by the right parenthesis ")" character that balances the left
parenthesis "(" character that began the part. If either a left or a right parenthesis occurs in
scheme data without being balanced by its counterpart, it must be escaped with a circumflex (^)
character preceding it. Escaping pairs of balanced parentheses is allowed. Any literal occurrences
of the circumflex must be escaped with an additional circumflex (that is, ^^). Any other use of a
circumflex is an error. */

//S* xmlns S* '(' S* XmlnsSchemeData S* ')'
u8vector_it
 URIParser::add_xmlns(bool& errors, xml::nsbindings_t& b, u8vector_it itbeg, u8vector_it const itend) const
{
    if(itbeg == itend) return itend;

    bmu::codepoint_iterator<u8vector_it> it(itbeg, itbeg, itend);
    bmu::codepoint_iterator<u8vector_it> const end(itend, itbeg, itend);

    static u32char_t const xmlns[] = { 'x', 'm', 'l', 'n', 's' };//ASCII
    static u32char_t const* const xmlns_end(xmlns + sizeof(xmlns)/sizeof(u32char_t));

    if( (it = std::search(it, end, xmlns, xmlns_end)) == end) {
        DBGMSGAT("Bad query part - no \"xmlns\"");
        return itend; //nema vise xmlns uparivanja, regularan kraj
    }
	
    std::advance(it, _countof(xmlns));//'(' S* XmlnsSchemeData S* ')
    DBGMSGAT("Initializing query part on: " << std::string(it.base(), itend));

    if((it = std::find(it, end, (u32char_t)'(')) == end) {
       DBGMSGAT("Bad query part - no \"(\")");
       return itend;
    }

	bmu::codepoint_iterator<u8vector_it> const part_end(
      std::find_if(it, end, bmu::match_closing<u32char_t, (u32char_t)'(', (u32char_t)')'>())
    );

    if(part_end == end) {
        DBGMSGAT("Bad query part - no \")\"");
        return itend;
    }

    ++it;

    DBGMSGAT("Working on query part: " << std::string(it.base(), part_end.base()));

    static bmu::match_NCNameStartChar const is_NCNameStartChar;
    it = std::find_if(it, part_end, is_NCNameStartChar);//prefix

    static bmu::match_NCNameChar const      is_NCNameChar;
    static std::unary_negate<bmu::match_NCNameChar> const not_NCNameChar(is_NCNameChar);

    u8vector_it prev;

    prev = it.base();
    it = std::find_if(it, part_end, not_NCNameChar);
    if(it == part_end) { //skip bad parts
        DBGMSGAT("Bad query part - no prefix");
        errors = true;
        return itend;
    }

	xml::nsbindings_t::value_type result(u8vector_t(prev, it.base()), u8vector_t());

    //skip white
    if((it = std::find(it, part_end, (u32char_t)'=')) == part_end) {//'='
        DBGMSGAT("Bad query part - no binding");
        errors = true;
        return itend;
    }

    //skip white
    it = std::find_if(++it, part_end, std::not1(bmu::match_White()));//namespace

    if(it == part_end) {
        DBGMSGAT("Query part OK - no namespace");//valid case
    }

    prev = it.base();
    it = std::find_if(it, part_end, bmu::match_White());
    result.second.assign(prev, it.base());

    //static std::pointer_to_unary_function<char const*, bool> const is_NamespaceChar(&URIParser::NamespaceChar);

    b.insert(result);

    return (++bmu::codepoint_iterator<u8vector_it>(part_end)).base();
}


bool URIParser::reset(std::vector<xml::nodestep_t>& npath, std::string const& nodeuri_percent) const
{
    npath.clear();

    u8vector_t nodeuri_nopercent;
    if(!percent_decode(nodeuri_nopercent, nodeuri_percent)) return false;

    validated(npath, nodeuri_nopercent.begin(), nodeuri_nopercent.end());

    return !npath.empty();
}


bool URIParser::reset(xml::nsbindings_t& prefixes, std::string const& query_percent) const
{
    prefixes.clear();

    u8vector_t query_nopercent;
    if(!percent_decode(query_nopercent, query_percent)) return false;

    bool errors(false);
    u8vector_it itbeg(query_nopercent.begin());
    u8vector_it const itend(query_nopercent.end());

    while(itbeg != itend) itbeg = add_xmlns(errors, prefixes, itbeg, itend);
    return !errors;
}


char URIParser::hex_digit_value(char const& c)
{
    if(c>='0' && c<='9') {
        return (c - '0');
    } else if(c>='a' && c<='f') {
        return (c - 'a' + 10);
    } else if(c>='A' && c<='F') {
        return (c - 'A' + 10);
    }
    DBGMSGAT("Invalid URI - percent digit is not valid hexadecimal digit");
    return 16;
}


/* XCAP Node selector:
 QName and AttValue allow many Unicode characters, outside of US-ASCII. When these characters need
 to be represented in the HTTP URI, they are percent-encoded. To do this, the data should be encoded
 first as octets according to the UTF-8 character encoding [18], and then only those octets that do
 not correspond to characters in the pchar set should be percent-encoded. For example, the character
 A would be represented as "A", the character LATIN CAPITAL LETTER A WITH GRAVE
*/
bool URIParser::percent_decode(u8vector_t& uri_out, std::string const& uri_percent)
{
    uri_out.clear();

    if(uri_percent.empty()) return true;

    char const* const uristart = &uri_percent[0];
    char const* const uriend(uristart+uri_percent.size());
    char const* const three_to_end(uriend-3);

    u8vector_t uri_nopercent;
    bool status(true);

    /* copy and url decode */
    for(char const* it = uristart; it != uriend; ++it) {
        if(*it == '\0') {
            DBGMSGAT("Invalid percent code - null");
            status = false;
            break;
        }

        if(*it != '%') {
            uri_nopercent.push_back(*it);
            continue;
        }

        if(it > three_to_end) { //%xx
            DBGMSGAT("Invalid percent code - hex too short");
            status = false;
            break;
        }

        char digit(hex_digit_value(*(++it)));
        if(digit == 16) {
            status = false;
            break;
        }
        uri_nopercent.push_back(digit << 4);

        digit = hex_digit_value(*(++it));
        if(digit == 16) {
            status = false;
            break;
        }
        *uri_nopercent.rbegin() += digit;
    }

    if(status) uri_out.swap(uri_nopercent);

    return status;
}


typedef boost::token_iterator<
    boost::char_separator<u8unit_t>, u8vector_t::const_iterator, u8vector_t
> u8vector_token_iterator;


bool URIParser::get_domain(u8vector_t& out, u8vector_t const& str)
{
    boost::char_separator<u8unit_t> const separator((u8unit_t*)"@");
    u8vector_token_iterator it(separator, str.begin(), str.end());
    u8vector_token_iterator const itend(separator, str.end(), str.end());
    if(it == itend || ++it == itend) return false;
    out = *it;
    return true;
}


bool URIParser::get_username(u8vector_t& out, u8vector_t const& str)
{
    boost::char_separator<u8unit_t> const separator((u8unit_t*)"@");
    u8vector_token_iterator it(separator, str.begin(), str.end());
    u8vector_token_iterator const itend(separator, str.end(), str.end());
    if(it == itend) return false;
    out = *it;
    return true;
}


bool URIParser::get_from_colon(u8vector_t& out, u8vector_t const& str)
{
    boost::char_separator<u8unit_t> const separator((u8unit_t*)":");
    u8vector_token_iterator it(separator, str.begin(), str.end());
    u8vector_token_iterator const itend(separator, str.end(), str.end());
    if(it == itend || ++it == itend) return false;
    out = *it;
    return true;
}


bool URIParser::reset(document_selector_t& docsel, u8vector_t& domain, u8vector_it itfrom, u8vector_it itto) const
{
    boost::char_separator<u8unit_t> const separator((u8unit_t*)"/");
    u8vector_token_iterator it(separator, itfrom, itto);
    u8vector_token_iterator const itend(separator, itto, itto);

    if(it == itend || it->empty()) {
        DBGMSGAT("Bad URI start");
        return false;
    }

    if(*it->rbegin() == ':') {
        DBGMSGAT("Looks like absolute URL");
        if(++it == itend || ++it == itend) { // 'http://xcap_root/'
            DBGMSGAT("No root");
            return false;
        }
        docsel.root = *it;
        ++it;
    } else {
        DBGMSGAT("Looks like relative URL");
        //if(++it == itend) return false;
        docsel.root.clear();//it's realative uri/path, xcap root is in Host HTTP request header
    }
    DBGMSGAT("Root is " << bmu::utf8_string(docsel.root));

    if(it == itend) {
        DBGMSGAT("No auid");
        return false;// '../auid/..'
    }
    docsel.auid = *it;
    DBGMSGAT("Auid is " << bmu::utf8_string(docsel.auid));

    if(docsel.auid.compare(0, 9, (u8unit_t*)"xcap-root") == 0) {
        DBGMSGAT("With openxcap domain trick");
        u8vector_it const itatend(docsel.auid.end());
        if(!get_domain(domain, docsel.auid)) return false;
        if(++it == itend) return false;
        docsel.auid = *it;
    } else {
        DBGMSGAT("Without openxcap domain trick");
        domain.clear();
    }

    if(++it == itend) {
        DBGMSGAT("No context");
        return false;// '../[global | users]/..'
    }
    docsel.context = *it;
    docsel.subtree = docsel.context;

    static u8vector_t const str_users((u8unit_t*)"users");
    static u8vector_t const str_global((u8unit_t*)"global");

    if(docsel.context == str_users) {

        DBGMSGAT("It's users context");

        if(++it == itend) {
            DBGMSGAT("No XUI");
            return false;// '../xui/..'
        }
        docsel.xui = *it;

        {
            u8vector_t tmp;
            if(get_from_colon(tmp, docsel.xui))// 'sip:user@domain'
                tmp.swap(docsel.xui);
        }

        u8vector_t domain_variant2;
        if(!get_domain(domain_variant2, docsel.xui)) {
            domain_variant2.clear();
        }

        {
            u8vector_t tmp;
            if(get_username(tmp, docsel.xui))// 'username@domain'
                tmp.swap(docsel.xui);
        }

        if(!domain.empty() && !domain_variant2.empty() && domain_variant2 != domain) {
            DBGMSGAT("Domain given in two ways but not same value");
            return false;//domain given in two ways but not same value
        }

        if(domain.empty())
            domain_variant2.swap(domain);

        docsel.subtree.push_back('/');
        docsel.subtree.append(docsel.xui);

    } else if(docsel.context != str_global) {

        DBGMSGAT("Bad context, not users and not global");
        return false;

    }

    if(domain.empty())
        domain.assign(default_domain.begin(), default_domain.end());//default domain

    if(++it == itend) {
        DBGMSGAT("No document name");
        return false;// '../docname'
    }
    docsel.docname = *it;

    if(++it != itend) {
        DBGMSGAT("Document selector have more steps then expected");
        return false;// visak koraka u uri
    }

    DBGMSGAT("Document selector parsed, OK");

    return true;
}


//prvi ~~ u URI ne bi trebao ali moze biti %-kodovan
bool URIParser::reset(
        document_selector_t& docpath
        , std::vector<xml::nodestep_t>& npath
        , u8vector_t& domain
        , std::string const& uri_percent
) const
{
    u8vector_t uri_nopercent;

    if(!percent_decode(uri_nopercent, uri_percent)) {
        DBGMSGAT("Invalid URI - failed percent decoding");
        return false;
    }

    DBGMSGAT("Percent decoded URI: " << bmu::utf8_string(uri_nopercent));

    static char const delimiter[] = "/~~/";
    u8vector_it doc_uri_end(
        std::search(uri_nopercent.begin(), uri_nopercent.end(), delimiter, delimiter+4)
    );

    if(!reset(docpath, domain, uri_nopercent.begin(), doc_uri_end)) {
        DBGMSGAT("Bad URI document selector - failed parsing");
        return false;
    }

    //Node selector - optional

    if(doc_uri_end == uri_nopercent.end()) {

        DBGMSGAT("URI OK - without node selector");
        validated(npath, doc_uri_end, doc_uri_end);

    } else {

        std::advance(doc_uri_end, 3);
        validated(npath, doc_uri_end, uri_nopercent.end());

        if(npath.empty()) {
            DBGMSGAT("Bad xpath in URI node selector - failed parsing xpath");
            return false;
        }
    }

    DBGMSGAT("URI OK");
    return true;
}

}
}
