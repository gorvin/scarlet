#pragma once
#include <xercesc/util/XercesDefs.hpp>

namespace XERCES_CPP_NAMESPACE {
	class DOMNode;
	class DOMDocument;
}

typedef xercesc::DOMNode     xmlnode_t;
typedef xercesc::DOMDocument xmldoc_t;

namespace scarlet {
namespace xml {
class XercesScope;
typedef boost::shared_ptr<XercesScope> XercesScopePtr;
}
}