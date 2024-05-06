#pragma once

#include "ref_counter.h"
#include "ref_policy_base.h"
#include "ref_policy_shared.h"

namespace coid
{

class ref_intrusive_base
{
    template<typename> friend class ref_intrusive;          // make all template instances friends
public: // methods only
protected: // methods only

    ref_intrusive_base()
    {
        create_counter_internal();
        _policy_ptr = ref_policy_shared<ref_intrusive_base>::create(this);
    }

    ref_intrusive_base(const ref_intrusive_base&) = delete;
    ref_intrusive_base& operator=(const ref_intrusive_base&) = delete;

    void create_counter_internal()
    {
        ref_counter::create(_counter_ptr);
    }

    void destroy_counter_internal()
    {
        ref_counter::destroy(_counter_ptr);
    }
protected: // members only
    ref_counter* _counter_ptr = nullptr;
    ref_policy_base* _policy_ptr = nullptr;
};

}; // end of namespace coid