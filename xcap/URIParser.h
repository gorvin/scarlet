#ifndef XCAP_URI_H
#define XCAP_URI_H
#include <scarlet/xcap/xcadefs.h>

namespace beam_me_up {
struct LexerChars;
}

namespace scarlet {

namespace xcap {

class URIParser {
	URIParser(void) = delete;
	
    static char hex_digit_value(char const& c);
    static bool percent_decode(u8vector_t& uri_nopercent, std::string const& uri_percent);
    static bool get_domain(u8vector_t& out, u8vector_t const& str);
    static bool get_username(u8vector_t& out, u8vector_t const& str);
    static bool get_from_colon(u8vector_t& out, u8vector_t const& str);

    bool is_valid_step(u8vector_it itbeg, u8vector_it const itend, xml::nodestep_t& nsel) const;
    bool is_attselector(u8vector_it itbeg, u8vector_it const itend, xml::nodestep_t& nsel) const;
    bool is_nsselector(u8vector_it itbeg, u8vector_it const itend) const;
    void validated(std::vector<xml::nodestep_t>& nsel, u8vector_it itbeg, u8vector_it const itend) const;

	//Options::instance().domain()
    bool reset(document_selector_t& docsel, u8vector_t& domain, u8vector_it itbeg, u8vector_it itend) const;

    u8vector_it add_xmlns(
        bool& errors
        , xml::nsbindings_t& prefixes
        , u8vector_it itbeg
        , u8vector_it const itend
    ) const;

public:
	URIParser(std::string const& default_domain);
    /** Parsira dio URLa iz prvog reda HTTP zahtjeva bez query dijela. URL se %-dekoduje, dijelom
        do delimitera '/~~/' se inicijalizuje xcap selektor dokumenta a ostatkom iz delimitera,
        ako postoji, se inicijalizuje xcap selektor cvora. Podrazumijeva se da se zadaje URL bez
        query dijela (dio od '?'), za query dio se koristi druga reset funkcija.
        Postoje dvije varijante URL-a u prvom dijelu zahtjeva, on moze biti:
         - apsolutni URL npr. https://xcap.example.com/auid/... ili
         - relativni URL odnosno path npr. /auid/.. pri cemu mora biti XCAP root URI u Host
           zaglavlju HTTP zahtjeva npr. 'Host: xcap.example.com'
        \return Ako je rezultat false onda je zadani URL los pa stanje izlaznih argumenata treba
        takodje smatrati losim iako je moguce da je u njima vec konzumiran dio URL-a.
        \remarks Ako je zadani URL relativni (path) onda ce docpath.root biti prazan pa ga treba
        naknadno postaviti na osnovu Host zaglavlja
    */
    bool reset(
        document_selector_t& docpath
        , std::vector<xml::nodestep_t>& npath
        , u8vector_t& domain//as openxcap trick ../xcap-root@domain/.. or from XUI or default
        , std::string const& uri_percent
    ) const;

    //ovo mi treba samo za testiranje jer inace se obradjuju u paru document i node selector
    bool reset(std::vector<xml::nodestep_t>& npath, std::string const& nodeuri_percent) const;

    /** Parsira query dio URLa iz prvog reda HTTP zahtjeva. Query se %-dekoduje i inicijalizuju
        uparivanja prefiksa sa namespaceima u izlaznom argumentu.
        \return Ako je rezultat false onda je zadani query los pa stanje izlaznog argumenta takodje
        treba smatrati losim iako je moguce da je u njemu vec konzumiran dio querya.
    */
    bool reset(xml::nsbindings_t& prefixes, std::string const& query_percent) const;

private:
	boost::shared_ptr<bmu::LexerChars> predicates;
	std::string default_domain;
};

}
}
#endif //XCAP_URI_H
