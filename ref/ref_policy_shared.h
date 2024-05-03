#pragma once
#include "ref_policy_base.h"

namespace coid
{


/// @brief The ref policy for the shared refs.
/// @note The ref_policy_shared instances are pooled.
/// @tparam Type - the type of the object the policy is countning the references for
template<typename Type>
class ref_policy_shared : public ref_policy_base
{
    using this_type = ref_policy_shared<Type>;
    using policy_pool_type = coid::pool<this_type>;
public: // methods only
    ref_policy_shared(const ref_policy_shared&) = delete;
    ref_policy_shared& operator=(const ref_policy_shared&) = delete;

    ref_policy_shared() = default;
    
    explicit ref_policy_shared(Type* original_ptr)
        : _original_ptr(original_ptr)
    {}
    

    static this_type* create(Type* object_ptr)
    {
        this_type* result = policy_pool_type::global().get_item();

        if (result == nullptr)
        {
            result = new this_type(object_ptr);
        }
        else 
        {
            result->_original_ptr = object_ptr;
        }

        result->_strong_counter.store(1, std::memory_order::memory_order_release);

        return result;
    }

    static this_type* create()
    {
        this_type* result = policy_pool_type::global().get_item();

        Type* object_ptr = new Type();
        
        if (result == nullptr)
        {
            result = new this_type(object_ptr);
        }

        result->_original_ptr = object_ptr;
        result->_strong_counter.store(1, std::memory_order::memory_order_release);

        return result;
    }

    void on_destroy() override
    {
        delete this->_original_ptr;
        this->_original_ptr = nullptr;

        this_type* t = this;
        policy_pool_type::global().release_item(t);
    }

    void* get_original_ptr() override
    {
        return _original_ptr;
    }

protected: // methods only
protected: // members only
    Type* _original_ptr;
};


}; // end of namespace coid