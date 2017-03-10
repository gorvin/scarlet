#include "XMLUni.h"
#include "scarlet/xml/XMLBase.h"
#include <xercesc/util/TransService.hpp>
#include <boost/shared_array.hpp>
#include <codecvt>

namespace scarlet {
namespace xml {

using xercesc::XMLTransService;
using xercesc::XMLPlatformUtils;
using xercesc::XMLString;

XMLTransService::Codes failReason8(XMLTransService::Ok);
XMLTransService::Codes failReason16(XMLTransService::Ok);

xml_engine_transcoder_t::xml_engine_transcoder_t(void)
 : trans_utf8(XMLPlatformUtils::fgTransService->makeNewTranscoderFor(xercesc::XMLUni::fgUTF8EncodingString, failReason8, BLOCK_SIZE))
// , trans_utf16(XMLPlatformUtils::fgTransService->makeNewTranscoderFor(xercesc::XMLUni::fgUTF16EncodingString, failReason16, BLOCK_SIZE))
{
    assert(trans_utf8.get());
    //assert(trans_utf16.get());
    assert(failReason8 == XMLTransService::Ok);
    //assert(failReason16 == XMLTransService::Ok);
}

xml_engine_transcoder_t::~xml_engine_transcoder_t() { }

//from UTF-8 etc. to internal xercesc codepage
bool xml_engine_transcoder_t::transcode_utf8(xmlstring& out, u8unit_t const* input, size_t rest) const
{
    if(!input || !rest) 
		return false;
    std::vector<unsigned char> chsize;//ne treba mi ali mora biti argument za transcodeFrom
    XMLSize_t byeaten(0);
    XMLSize_t fCharsWritten(0);
    while(rest) {
        out.resize(out.size() + rest);
        chsize.resize(out.size());
		try {
			fCharsWritten += trans_utf8->transcodeFrom(input, rest, &out[0] + fCharsWritten
				, out.size() - fCharsWritten, byeaten, &chsize[0]);
		} 
		catch(...) {
			return false;
		}
        if(byeaten == 0) 
			return false;
        rest -= byeaten;
        input += byeaten;
    }
    out.resize(fCharsWritten);
    return true;
}

//from internal xercesc codepage to UTF-8 etc.
bool xml_engine_transcoder_t::transcode_utf8(u8vector_t& out, XMLCh const* input, size_t rest) const
{
    if(!input || !rest) 
		return false;
    XMLSize_t charseaten(0);
    XMLSize_t fBytesWritten(0);
    while(rest) {
        out.resize(out.size() + rest*sizeof(XMLCh));
		try {
			fBytesWritten += trans_utf8->transcodeTo(input, rest, &out[0] + fBytesWritten
				, out.size() - fBytesWritten, charseaten, xercesc::XMLTranscoder::UnRep_Throw);
		} 
		catch (...) {
			return false;
		}
        if(charseaten == 0) 
			return false;
        rest -= charseaten;
        input += charseaten;
    }
    out.resize(fBytesWritten);
    return true;
}

bool xml_engine_transcoder_t::transcode(xmlstring& xstr, char const* cstr) const
{
    boost::shared_array<XMLCh> result(XMLString::transcode(cstr));
    if(!result.get()) 
		return false;
    xmlstring(result.get()).swap(xstr);
    return true;
}

bool xml_engine_transcoder_t::transcode(std::string& str, XMLCh const* cstr) const
{
    boost::shared_array<char> result(XMLString::transcode(cstr));
    if(!result.get()) 
		return false;
    std::string(result.get()).swap(str);
    return true;
}

u8vector_t stdtranscode(std::wstring const& source)
{
	typedef std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16;
	return bmu::utf8_vector(utf8_utf16().to_bytes(source));
}

u8vector_t stdtranscode(wchar_t const* source)
{
	typedef std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16;
	return bmu::utf8_vector(utf8_utf16().to_bytes(source));
}

std::wstring stdtranscode(std::string const& source)
{
	typedef std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16;
	return utf8_utf16().from_bytes(source);
}

std::wstring stdtranscode(char const* source)
{
	typedef std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16;
	return utf8_utf16().from_bytes(source);
}

std::wstring stdtranscode(u8vector_t const& source)
{
	return source.empty() ? std::wstring() : stdtranscode(&source[0]);
}

std::wstring stdtranscode(u8unit_t const* source)
{
	typedef std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16;
	return utf8_utf16().from_bytes((char const*)source);
}

}
}
