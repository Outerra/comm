#pragma once

#include "../commtypes.h"
#include "../binstream/binstream.h"
#include "ref_policy_base.h"

namespace coid {
/// @brief Template class for uniquely owned object with policy for creation/destruction of the object.
/// @tparam Type type of referenced object
/// @note The policy is optional. Only move operation is permitted. Can be moved to ref_shared.
/// @note Default behaviour of the instance without policy is as if the  ref_policy_simple was used.
/// @note When not using any policy and the move to ref_shared is called, the ref_policy_simple is used
template <class Type>
class ref_unique
{
    template<typename> friend class ref_unique;     // make all template instances friends
public: // methods only
    /// @brief  Default constructor creates empty ref_unique
    ref_unique() = default;
    
    ref_unique(nullptr_t)
    {};

    /// @brief  Destructor
    ~ref_unique() 
    { 
        release();
    }

    /// @brief Constructor from raw pointer of Type or DerivedType
    /// @param object_ptr - object pointer
    template <class BaseOrDerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    ref_unique(BaseOrDerivedType* object_ptr)
    { 
        _object_ptr = object_ptr; 
    }

    /// @brief Copy constructor is deleted. Only move constructor is available for ref_unique
    ref_unique(const ref_unique& object_ptr) = delete;

    /// @brief Move constructor
    template <class BaseOrDerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    ref_unique(ref_unique<BaseOrDerivedType>&& rhs)
    {
        _policy_ptr = rhs._policy_ptr;
        _object_ptr = rhs._object_ptr;
        rhs._policy_ptr = nullptr;
        rhs._object_ptr = nullptr;
    }

    /// @brief Construct object with constructor with given args
    /// @param ...args - Argumes of Type consturctor
    /// @return 
    template <typename BaseOrDerivedType = Type, typename... Args>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    static ref_unique<Type> emplace(Args&&... args) {
        return ref_unique<Type>(new BaseOrDerivedType(static_cast<Args&&>(args)...));
    }

    ///// @brief Create object with default constructor without policy
    template<typename BaseOrDerivedType = Type>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    void create()
    {
        _object_ptr = new BaseOrDerivedType();
    }

    /// @brief Create method with default object construction determined by used policy
    /// @tparam Policy - must be base of ref_policy_base
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy<Policy>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(_policy_ptr->get_original_ptr());
    }

    /// @brief Create method with default object construction determined by used policy
    /// @tparam DerivedType - type derived from Type
    /// @tparam Policy - must be base of ref_policy_base
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    template<typename BaseOrDerivedType,typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::is_base_of<Type, BaseOrDerivedType>::value && is_ref_policy<Policy>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<BaseOrDerivedType*>(_policy_ptr->get_original_ptr()));
    }

    /// @brief Get pointer of referenced object
    /// @return object pointer or nullptr if empty
    Type* get() const 
    { 
        return _object_ptr; 
    }

    Type*& get_ptr_ref() { return _object_ptr; }

    explicit operator bool() const { return _object_ptr != 0; }

    int operator ==(const Type* ptr) const { return ptr == _object_ptr; }
    int operator !=(const Type* ptr) const { return ptr != _object_ptr; }

    Type& operator *(void) { DASSERT(_object_ptr != nullptr); return *_object_ptr; }
    const Type& operator *(void) const { DASSERT(_object_ptr != nullptr); return *_object_ptr; }

    Type* operator ->(void) { DASSERT(_object_ptr != nullptr); return _object_ptr; }
    const Type* operator ->(void) const { DASSERT(_object_ptr != nullptr); return _object_ptr; }

    /// @brief Assigment operator from Type pointer
    /// @param object_ptr - pointer to the object to reference
    ref_unique& operator= (Type* object_ptr) {
        release();

        _object_ptr = object_ptr;
        return *this;
    }

    /// @brief Move assignment operator for base type
    /// @param rhs - right hand side
    ref_unique& operator=(ref_unique&& rhs)
    {
        if (&rhs != this)
        {
            release();
            _policy_ptr = rhs._policy_ptr;
            _object_ptr = rhs._object_ptr;
            rhs._policy_ptr = nullptr;
            rhs._object_ptr = nullptr;
        }

        return *this;
    }

    /// @brief Move assignment operator for derived type
    /// @tparam DerivedType - type derived from Type
    /// @param rhs - right hand side
    template<typename DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    ref_unique& operator=(ref_unique<DerivedType>&& rhs)
    {
        release();

        _policy_ptr = rhs._policy_ptr;
        _object_ptr = static_cast<Type*>(rhs._object_ptr);
        rhs._policy_ptr = nullptr;
        rhs._object_ptr = nullptr;
        return *this;
    }

    /// @brief Tests if this reference is set (!= nullptr)
    /// @return true if is set
    bool is_set() const 
    { 
        return _object_ptr != nullptr; 
    }

    /// @brief Tests if this reference is empty (== nullptr)
    /// @return true if is empty
    bool is_empty() const 
    { 
        return _object_ptr == nullptr; 
    }

    /// @brief Helper for move
    /// @return rvalue of ThisType
    ref_unique&& move() { return static_cast<ref_unique&&>(*this); }

    /// @brief Destroys referenced object and sets reference to nullptr
    void release()
    {
        if (_policy_ptr != nullptr)
        {
            DASSERT(_policy_ptr->_counter.get_strong_counter_value() == 0);
            _policy_ptr->on_destroy();
            _policy_ptr = nullptr;
        }
        else if (_object_ptr)
        {
            delete _object_ptr;
        }

        _object_ptr = nullptr;
    }

    Type* eject()
    {
        if (_policy_ptr != nullptr)
        {
            return nullptr;
        }
        else 
        {
            Type* result = _object_ptr;
            _object_ptr = nullptr;
            return result;
        }
        
    }

    /// @brief Swaps content with given ref_unique
    /// @param other - ref_unique to swap with
    void swap(ref_unique& other)
    {
        std::swap(_object_ptr, other._object_ptr);
        std::swap(_policy_ptr, other._policy_ptr);
    }

    /// @brief Swaps content of two ref_unique
    /// @param a - first ref_unique
    /// @param b - second ref_unique
    friend void swap(ref_unique& a, ref_unique& b) {
        std::swap(a._object_ptr, b._object_ptr);
        std::swap(a._policy_ptr, b._policy_ptr);
    }
protected: // members only
    ref_policy_base* _policy_ptr = nullptr;
    Type* _object_ptr = nullptr;
};

}; // end of namespace coid