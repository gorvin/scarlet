#include "scarlet/http/SvcHandler.h"

namespace scarlet
{
namespace http
{
SvcHandler::SvcHandler(std::vector<std::string> const& resources)
	: m_resources(resources)
{ }
}
}
