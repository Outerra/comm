#pragma once

#include "ref_counter.h"
#include "ref_policy_base.h"
#include "ref_policy_simple.h"

namespace coid
{

class ref_intrusive_base
{
    template<typename Type> friend class ref_intrusive;          // make all template instances friends
public: // methods only
    virtual ~ref_intrusive_base()
    {
        _policy_ptr = nullptr;
    };

    void decrease_strong_counter()
    {
        DASSERT_RET(_policy_ptr);
        _policy_ptr->_counter.decrease_strong_counter();
    }

    void increase_strong_counter()
    {
        DASSERT_RET(_policy_ptr);
        _policy_ptr->_counter.increase_strong_counter();
    }
protected: // methods only
    ref_intrusive_base() = default;
    ref_intrusive_base(const ref_intrusive_base&) = delete;
    ref_intrusive_base& operator=(const ref_intrusive_base&) = delete;
protected: // members only
    ref_policy_base* _policy_ptr = nullptr;
};

}; // end of namespace coid