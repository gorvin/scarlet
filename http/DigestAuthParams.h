#include <string>

namespace scarlet {

class DigestAuthParams {
	std::string username;
	std::string realm;
	std::string algorithm;
	std::string cnonce;
    std::string uri;
    std::string qop;
    std::string nc;
    std::string nonce;
    std::string response;

    static bool is_white(char const c);
    static std::string trim_whites(std::string const& str);
    static std::string unquote(std::string const& str_nottrimed);
    void calc_ha1(std::string& ha1, std::string const& domain, std::string const& password) const;
    void calc_ha2(std::string& ha2, std::string const& method) const;
    void calc_response(std::string& expected, std::string const& ha1, std::string const& ha2) const;
public:
    bool add_parameter(std::string const& name, std::string const& value);
    std::string const& get_username(void) const { return username; }
	std::string const& get_realm(void) const { return realm; }
	std::string const& get_algorithm(void) const { return algorithm; }
	std::string const& get_nonce(void) const { return nonce; }
    std::string const& get_uri(void) const { return uri; }
    //varijanta u kojoj se svaki put racuna ha1
    bool check(std::string const& method, std::string const& domain, std::string const& password) const;
    //varijanta u kojoj se koristi poznat ha1 npr. kad se cuva u Storage
    bool check(std::string const& method, std::string const& ha1) const;
};

}
