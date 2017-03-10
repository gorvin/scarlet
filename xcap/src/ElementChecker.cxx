#include "scarlet/xcap/ElementChecker.h"

namespace scarlet {
namespace xcap {

ValExtractFn MakaAttributeExtractor(u8vector_t const& name)
{
	return boost::bind(ExtractAttibuteByName, boost::cref(name), boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3);
}

void XCAPError::append_selector_to(u8vector_t& doc) const
{
    std::map<u8vector_t, u8vector_t> allnamespaces;

    {
        //nadji sve koristene XML prostore imena steps
        std::list<stepinfo_t>::const_iterator it(steps.begin());
        for( ; it != steps.end(); ++it) {
            if(!it->node_ns.empty())
                allnamespaces[it->node_ns] = u8vector_t();
        }
    }
    {
        //pridruzi koristenim prostorima imena genericki prefix oblika nsN
        std::map<u8vector_t, u8vector_t>::iterator it_ns(allnamespaces.begin());
        for(size_t suffix(1); it_ns != allnamespaces.end(); ++it_ns) {
            std::string const sufixstr(std::string("ns") + bmu::to_string(suffix, std::dec));
            it_ns->second = u8vector_t(sufixstr.begin(), sufixstr.end());
        }
    }
    {
        //formiraj node selector koristeci pridruzene prefikse mjesto koristenih prostora imena
        std::list<stepinfo_t>::const_iterator it(steps.begin());
        for( ; it != steps.end(); ++it) {
            doc.push_back('/');
            if(!it->node_ns.empty()) {
                doc.append(allnamespaces[it->node_ns]).push_back(':');
            }
            doc.append(it->node_name).push_back('[');
            std::string const posstr(bmu::to_string(it->position, std::dec));
            doc.append(posstr.begin(), posstr.end()).push_back(']');
        }
    }
    {
        //dodaj node selectoru query sa XPointer izrazom (uparivanje prefiksa sa prostorima imena)
        doc.push_back('?');
        std::map<u8vector_t, u8vector_t>::iterator it_ns(allnamespaces.begin());
        for( ; it_ns != allnamespaces.end(); ++it_ns) {
            doc.append((u8unit_t const*)"xmlns(");
            doc.append(it_ns->second.begin(), it_ns->second.end());
            doc.push_back('=');
            doc.append(it_ns->first.begin(), it_ns->first.end());
            doc.push_back(')');
        }
    }
}



void FailUniqueness::write(u8vector_t& doc, u8vector_t const& root) const
{
    doc.clear();
    doc.append((u8unit_t const*)
        "  <uniqueness-failure>\n"
        "    <exists phrase=\""
    );

    XCAPError::append_message_to(doc);

    doc.append((u8unit_t const*)"\" field=\"/").append(root);

    XCAPError::append_selector_to(doc);

    doc.append((u8unit_t const*)
        "\"/>\n"
        "  </uniqueness-failure>"
    );
}


void FailConstraint::write(u8vector_t& doc, u8vector_t const& root) const
{
    doc.clear();
    doc.append((u8unit_t const*)
        "  <constraint-failure phrase=\"Constraints failed for element '/"
    ).append(root);

    XCAPError::append_selector_to(doc);

    doc.append((u8unit_t const*)"' with reason: ");

    XCAPError::append_message_to(doc);

    doc.append((u8unit_t const*)"\"/>");
}


XCAPErrorPtr ElementChecker::check(xmlnode_t const* node, xcapuri_t const& rquri)
{
    ++errstep.position;

	xml::XMLSelect selection(const_cast<xmlnode_t*>(node));

    for(size_t i(0); i<partial_checks.size(); ++i)
    {
        XCAPErrorPtr error(partial_checks[i](this, selection, rquri));
        if(error)
            return error;
    }

    if(recursive) {
        XCAPErrorPtr error(recursive(selection, rquri));
        if(error) {
            //u rekurziji je objekat istog tipa zapoceo gresku na istom xerror, ovdje nastavljas
            error->prev_step(errstep);
            return error;
        }
    }

    return ElementChecker::ok();
}


XCAPErrorPtr UniqueValCheck::operator()(ElementChecker const* checker, xml::XMLSelect const& selection, xcapuri_t const& rquri)
{
    XCAPErrorPtr error;

    u8vector_t value;
    error = valextract(checker, selection, value);
    if(error) return error;

    if(value.empty() || std::find(allvalues.begin(), allvalues.end(), value) != allvalues.end())
        return checker->fail_uniqueness();

    for(size_t i(0); i<valchecks.size(); ++i) {
        error = (valchecks[i])(checker, value, rquri);
        if(error) return error;
    }

    allvalues.push_back(value);

    return ElementChecker::ok();
}


XCAPErrorPtr ExtractAttibuteByName(u8vector_t const& name, ElementChecker const* checker, xml::XMLSelect const& selection, u8vector_t& value)
{
    xmlnode_t* const attr(selection.attribute(name));

    if(!attr) { //mandatory
        return checker->fail_constraint(
            std::string("attribute '").append(name.begin(), name.end()).append("' is mandatory")
        );
    }

	xml::XMLSelect(attr).value(value);
    return ElementChecker::ok();
}


XCAPErrorPtr ChildValueExtractor(ElementChecker const* checker, xml::XMLSelect const& selection, u8vector_t& value)
{
	xml::XMLSelect childuri(selection.first_child());

    if(!childuri.node()) { //mandatory
        return checker->fail_constraint("Must contain child text node with URI reference as value");
    }

    childuri.value(value);
    return ElementChecker::ok();
}

}
}
