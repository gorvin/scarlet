#ifndef SCARLET_XCADEFS_H
#define SCARLET_XCADEFS_H
#include <scarlet/xml/XMLSelect.h>
#include <bmu/exdefs.h>

namespace scarlet {
namespace xcap {
using ::bmu::u8unit_t;
using ::bmu::u8vector_t;
using ::bmu::u8vector_it;
using ::bmu::rawcontent_t;

/** Dio XCAP URIja bez selektora ?vora i bez upita tj. do /~~/ ako ima, ina?e do ? ako ima ili ?itav URI*/
struct document_selector_t {
	u8vector_t root;//xcap.example.com
	u8vector_t auid;//IETF: xcap-caps, resource-list ... VENDOR: com.example.foo ...
	u8vector_t context;//global or users
	u8vector_t xui;//empty or xui e.g. sip:joe@foo.com
	u8vector_t subtree;//global or users/xui e.g. users/sip:joe@foo.com
	u8vector_t docname;//myFriends.xml
};

inline std::string printable(document_selector_t const& dpath) {
	return bmu::utf8_string(
		u8vector_t(dpath.root).append((u8unit_t*)"/")
		.append(dpath.auid).append((u8unit_t*)"/")
		.append(dpath.subtree).append((u8unit_t*)"/")
		.append(dpath.docname)
		);
}

/** Predstavljanje ?itavog XCAP URIja. */
struct xcapuri_t {
	document_selector_t          docpath;/**< Document selector. */
	std::vector<xml::nodestep_t> npath;/**< Node selector. */
	xml::nsbindings_t            prefixes;/**< Query. */
};

struct storage_error : virtual bmu::exception_t { };
struct invalid_stored_document : virtual bmu::exception_t { };

}
}
#endif // SCARLET_XCADEFS_H