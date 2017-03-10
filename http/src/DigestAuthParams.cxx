#include "scarlet/http/DigestAuthParams.h"
#include "bmu/MD5Calc.h"
#include "bmu/Logger.h"
#include <boost/token_iterator.hpp>

namespace scarlet {

/* For the purposes of document RFC 2617 (HTTP Authentication), an MD5 digest of 128 bits is
represented as 32 ASCII printable characters. The bits in the 128 bit digest are converted from most
significant to least significant bit, four bits at a time to their ASCII presentation as follows.
Each four bits is represented by its familiar hexadecimal notation from the characters
0123456789abcdef. That is, binary 0000 gets represented by the character '0', 0001, by '1', and so
on up to the representation of 1111 as 'f'.
*/
void hex_digits(std::string& out, unsigned char const* arr, size_t arr_size)
{
    out.clear();
    static const char digits[] = "0123456789abcdef";//NOTE: lower case
    for(size_t i(0); i<arr_size; ++i) {
        unsigned char const& c(arr[i]);
        out.push_back(digits[(unsigned char)(c>>4)]);
        out.push_back(digits[(unsigned char)(c&0x0f)]);
    }
}

bool DigestAuthParams::is_white(char const c)
{
    switch(c) {
    case ' ': case '\t': case 0x0d: case 0x0a:
        return true;
    default:
        return false;
    }
}

std::string DigestAuthParams::trim_whites(std::string const& str)
{
    if(str.empty()) return str;
    size_t start(0);
    while(start != str.size() && is_white(str[start])) ++start;//find first non-white before
    if(start == str.size()) {
        return std::string();
    }
    size_t end(start);
    while(end != str.size() && !is_white(str[end])) ++end;//find first white after
    return str.substr(start, end-start);
}

std::string DigestAuthParams::unquote(std::string const& str_nottrimed)
{
    std::string str(trim_whites(str_nottrimed));
    if(str.size()>1 && str[0] == '\"' && *str.rbegin() == '\"') {
        return str.substr(1, str.size()-2);
    }
    return str;
}

bool DigestAuthParams::add_parameter(std::string const& rawname, std::string const& rawvalue)
{
    std::string name(trim_whites(rawname));
    std::string value(unquote(rawvalue));
    if(name == "username") {
        username = value;
    } else if(name == "realm") {
		realm = value;
    } else if(name == "algorithm") {
		algorithm = value;
    } else if (name == "uri") {
        uri = value;
    } else if (name == "qop") {
        qop = value;
    } else if (name == "cnonce") {
        cnonce = value;
    } else if (name == "nonce") {
        nonce = value;
    } else if (name == "nc") {
        nc = value;
    } else if (name == "response") {
        response = value;
    } else {
        DBGMSGAT("Bad Digest credentials format, invalid parameter name '" << name.c_str() << "' with value '" << value.c_str() << "'");
        return false;
    }
    return true;
}

void DigestAuthParams::calc_ha1(
    std::string& ha1
    , std::string const& domain
    , std::string const& password
) const
{
	bmu::MD5Calc calc;
    calc.Update(username.data(), username.size());
    calc.Update(":", 1);
    calc.Update(domain.data(), domain.size());
    calc.Update(":", 1);
    calc.Update(password.data(), password.size());
    unsigned char digest[16];
    calc.Finish(digest);
    hex_digits(ha1, digest, 16);
}

void DigestAuthParams::calc_ha2(std::string& ha2, std::string const& method) const
{
	bmu::MD5Calc calc;
    calc.Update(method.data(), method.size());
    calc.Update(":", 1);
    calc.Update(uri.data(), uri.size());
    //TODO: if qop == "auth-int" update(:), update(MD5(body))
    unsigned char digest[16];
    calc.Finish(digest);
    hex_digits(ha2, digest, 16);
}

void DigestAuthParams::calc_response(std::string& expected, std::string const& ha1, std::string const& ha2) const
{
	bmu::MD5Calc calc;
    calc.Update(ha1.data(), ha1.size());
    calc.Update(":", 1);
    calc.Update(nonce.data(), nonce.size());
    //TODO: if qop != NULL {
        calc.Update(":", 1);
        calc.Update(nc.data(), nc.size()); //nonce_count
        calc.Update(":", 1);
        calc.Update(cnonce.data(), cnonce.size());
        calc.Update(":", 1);
        calc.Update(qop.data(), qop.size());
    // }
    calc.Update(":", 1);
    calc.Update(ha2.data(), ha2.size());
    unsigned char digest[16];
    calc.Finish(digest);
    hex_digits(expected, digest, 16);
}

bool DigestAuthParams::check(
    std::string const& method
    , std::string const& domain
    , std::string const& password
) const
{
    if(response.size() != 32) {
        return false;
    }
    //TODO: if !qop.empty && qop != "auth" && qop != "auth-int" DBGMSGAT("Unknown quality of protection")
    if(username.empty()) return false;
    std::string ha1;
    calc_ha1(ha1, domain, password);
    std::string ha2;
    calc_ha2(ha2, method);
    std::string expected_response;
    calc_response(expected_response, ha1, ha2);
    std::string response_lcase(response);
    for(size_t i(0); i<32; ++i) response_lcase[i] |= 0x20;//0010 0000
    return response_lcase == expected_response;
}

bool DigestAuthParams::check(std::string const& method, std::string const& ha1) const
{
    assert(ha1.size() == 32);
    if(response.size() != 32) {
        return false;
    }
    //TODO: if !qop.empty && qop != "auth" && qop != "auth-int" DBGMSGAT("Unknown quality of protection")
    std::string ha2;
    calc_ha2(ha2, method);
    std::string expected_response;
    calc_response(expected_response, ha1, ha2);
    std::string response_lcase(response);
    for(size_t i(0); i<32; ++i) response_lcase[i] |= 0x20;//0010 0000
    return response_lcase == expected_response;
}

}
