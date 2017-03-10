#include <bmu/tydefs.h>
#include <xercesc/util/XercesDefs.hpp>
#include <boost/scoped_ptr.hpp>

namespace XERCES_CPP_NAMESPACE {
    class XMLTranscoder;
    class XMLTransService;
};

namespace scarlet {
namespace xml {
using ::bmu::u8unit_t;
using ::bmu::u8vector_t;
using ::bmu::u8vector_it;

/** Samo da olakša rad sa Xercesovim nizovima XMLCh a važe iste napomene kao za \ref u8vector_t. */
typedef std::basic_string<XMLCh>     xmlstring;

/* Is possible to use an XMLTranscoder obtained from XMLPlatformUtils::fgTransService->makeNewTranscoderFor
in multiple threads at the same time?

In general, no.  Some of the transcoders are thread-safe safe, depending on the transcoding service selected,
because they don't keep any state  information.  However, it would be a very bad idea to rely on an
undocumented "feature" like this.

The ICU User Guide discusses this, for all objects that have an open/close model. Each Converter object must
be used in a single thread at a time. If you need more of them, clone them. They're cheap to clone.

Xerces encodes information as UTF-16 internally. The UTF-16 data is stored using the XMLCh datatype.
*/
class xml_engine_transcoder_t {
    enum { BLOCK_SIZE = 2048 };
	boost::scoped_ptr<xercesc::XMLTranscoder> trans_utf8;
    //boost::scoped_ptr<xercesc::XMLTranscoder> trans_utf16;
public:
    //from UTF-8 etc. to internal xecesc text encoding (UTF-16)
    bool transcode_utf8(xmlstring& out, u8unit_t const* input, size_t rest) const;
    bool transcode_utf8(xmlstring& out, u8vector_t const& input) const
    {
        return transcode_utf8(out, input.data(), input.size());
    }
	xmlstring transcode_utf8(u8unit_t const* cstr) const
	{
		xmlstring xstr;
		return transcode_utf8(xstr, cstr) ? xstr : xmlstring();
	}
	xmlstring transcode_utf8(char const* cstr) const
	{
		xmlstring xstr;
		return transcode_utf8(xstr, (u8unit_t const*)cstr) ? xstr : xmlstring();
	}
    //from internal xercesc text encoding (UTF-16) to UTF-8 etc.
    bool transcode_utf8(u8vector_t& out, XMLCh const* input, size_t rest) const;
    bool transcode_utf8(u8vector_t& out, xmlstring const& input) const
    {
        return transcode_utf8(out, input.data(), input.size());
    }
	u8vector_t transcode_utf8(XMLCh const* cstr) const
	{
		u8vector_t u8str;
		return transcode_utf8(u8str, cstr) ? u8str : u8vector_t();
	}
	//from native (system) codepage to internal xercesc text encoding (UTF-16)
    bool transcode(xmlstring& xstr, char const* cstr) const;
    bool transcode(xmlstring& xstr, std::string const& str) const
    {
        return transcode(xstr, str.c_str());
    }
    xmlstring transcode(char const* cstr) const
    {
        xmlstring xstr;
        return transcode(xstr, cstr) ? xstr : xmlstring();
    }
    xmlstring transcode(std::string const& str) const
    {
        return transcode(str.c_str());
    }
    //from internal xercesc text encoding (UTF-16) to native (system) codepage
    bool transcode(std::string& str, XMLCh const* cstr) const;
    bool transcode(std::string& str, xmlstring const& xstr) const
    {
        return transcode(str, xstr.c_str());
    }
    std::string transcode(XMLCh const* cstr) const
    {
        std::string str;
        return transcode(str, cstr) ? str : std::string();
    }
    std::string transcode(xmlstring const& xstr) const
    {
        return transcode(xstr.c_str());
    }
	xml_engine_transcoder_t(void);
    ~xml_engine_transcoder_t();
};

#define _TRUTF8 tr()->transcode_utf8
//#define _TRUTF16 tr()->transcode_utf16
//#define _TRLCP tr()->transcode
#define _TRLCP stdtranscode

xml_engine_transcoder_t const* tr(void);
void xml_engine_exception_handler(void);

u8vector_t stdtranscode(std::wstring const& source);
u8vector_t stdtranscode(wchar_t const* source);
std::wstring stdtranscode(std::string const& source);
std::wstring stdtranscode(char const* source);
std::wstring stdtranscode(u8vector_t const& source);
std::wstring stdtranscode(u8unit_t const* source);

}
}
