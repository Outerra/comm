#include "parent.hpp"

namespace a
{
namespace b
{
namespace c
{

class child_class : public a::b::parent_class
{
public: // methods only
    child_class(const child_class&) = delete;
    child_class& operator=(const child_class&) = delete;
    child_class(int value);

    ifc_class(a::b::c::child_class_ifc : a::b::parent_class_ifc, "ifc", "parent.hpp");
    ifc_fn static iref<a::b::c::child_class> _get(void* ptr);

protected: // methods only
protected: // members only
};


}; // end of namespace c
}; // end of namespace b
}; // end of namespace a