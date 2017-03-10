#include "scarlet/xcap/xcadefs.h"
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <list>

namespace scarlet {
namespace xcap {

struct stepinfo_t {
    u8vector_t node_ns;//namespace, prefix will be generated on writing full error report
    u8vector_t node_name;
    size_t     position;
    stepinfo_t(u8vector_t const& node_ns, u8vector_t const& node_name, size_t position)
     : node_ns(node_ns)
     , node_name(node_name)
     , position(position)
     { }
private:
    explicit stepinfo_t(void); // NE
};

class XCAPError {
    std::string const       message;
    std::list<stepinfo_t>   steps;
    explicit XCAPError(void); // NE
protected:
    XCAPError(std::string const& message, stepinfo_t const& last_step)
     : message(message)
     , steps(1, last_step)
     { }
    virtual ~XCAPError() { }
    void append_selector_to(u8vector_t& docresponse) const;
    void append_message_to(u8vector_t& docresponse) const
    {
        docresponse.append(message.begin(), message.end());
    }
public:
    virtual void write(u8vector_t& docresponse, u8vector_t const& root_name) const = 0;

    /** \param idx Index accross same named siblings. */
    void prev_step(stepinfo_t const& step) { steps.push_front(step); }
};

typedef boost::shared_ptr<XCAPError> XCAPErrorPtr;

class FailUniqueness : public XCAPError {
    void write(u8vector_t& docresponse, u8vector_t const& root_name) const;
public:
    FailUniqueness(std::string const& phrase, stepinfo_t const& last_step)
     : XCAPError(phrase, last_step)
     { }
};

class FailConstraint : public XCAPError {
    void write(u8vector_t& docresponse, u8vector_t const& root_name) const;
public:
    FailConstraint(std::string const& phrase, stepinfo_t const& last_step)
     : XCAPError(phrase, last_step)
     { }
};

class ElementChecker;
//Parcijalna provjera restrikcija, samo jedna restrikcija, provjera samo jednog atributa ili djeteta
typedef boost::function<XCAPErrorPtr(ElementChecker const*, xml::XMLSelect const&, xcapuri_t const&)> PartialCheckFn;
typedef boost::function<XCAPErrorPtr(xml::XMLSelect const&, xcapuri_t const&)> RecursiveCheckFn;

class ElementChecker {
    boost::shared_ptr<xml::match_prefixed_name> const    is_element_to_check;
	RecursiveCheckFn                                recursive;
    std::vector<PartialCheckFn>                     partial_checks;
    stepinfo_t                                      errstep;
public:
    ElementChecker(
        boost::shared_ptr<xml::match_prefixed_name> is_element_to_check
        , RecursiveCheckFn rchk = RecursiveCheckFn()
    )
     : is_element_to_check(is_element_to_check)
     , recursive(rchk)
     , errstep(is_element_to_check->el_ns(), is_element_to_check->el_name(), 0)
     { }
    ~ElementChecker() { }
    void push(PartialCheckFn chk) { partial_checks.push_back(chk); }
	bool match(xmlnode_t const* node) const { return (*is_element_to_check)(node); }
    XCAPErrorPtr check(xmlnode_t const* node, xcapuri_t const& rquri);
    XCAPErrorPtr fail_uniqueness(std::string const& phrase = std::string()) const
    {
        return XCAPErrorPtr(new FailUniqueness(phrase, errstep));
    }
    XCAPErrorPtr fail_constraint(std::string const& phrase) const
    {
        return XCAPErrorPtr(new FailConstraint(phrase, errstep));
    }
    static XCAPErrorPtr ok(void)
    {
        return XCAPErrorPtr();//NULL
    }
};

typedef boost::function<XCAPErrorPtr(ElementChecker const*, u8vector_t const&, xcapuri_t const&)> ValConstraintFn;
typedef boost::function<XCAPErrorPtr(ElementChecker const*, xml::XMLSelect const&, u8vector_t&)> ValExtractFn;

class UniqueValCheck {
    explicit UniqueValCheck(void) = delete; //
    std::list<u8vector_t>           allvalues;//accross element siblings
	ValExtractFn                    valextract;
    std::vector<ValConstraintFn>    valchecks;
public:
	XCAPErrorPtr operator()(ElementChecker const*, xml::XMLSelect const& selection, xcapuri_t const& rquri);
	void push(ValConstraintFn chk) { valchecks.push_back(chk); }
    UniqueValCheck(ValExtractFn valextract)
     : valextract(valextract)
     { }
};

XCAPErrorPtr ExtractAttibuteByName(u8vector_t const& name, ElementChecker const* checker, xml::XMLSelect const& selection, u8vector_t& value);
ValExtractFn MakaAttributeExtractor(u8vector_t const& name);
XCAPErrorPtr ChildValueExtractor(ElementChecker const*, xml::XMLSelect const& selection, u8vector_t& value);

typedef boost::shared_ptr<ElementChecker> ElementCheckerPtr;
}
}