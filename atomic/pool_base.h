#ifndef __COMM_ATOMIC_POOL_BASE_H__
#define __COMM_ATOMIC_POOL_BASE_H__

#include "../namespace.h"
#include "../alloc/commalloc.h"
//#include "..ref/ref_policy_base.h"
#include "pool.h"

COID_NAMESPACE_BEGIN

/////
//template<class T>
//class pool
//    : protected atomic::stack_base<T>
//{
//    using stack_t = atomic::stack_base<T>;
//
//public:
//    pool()
//        : atomic::stack_base<T>()
//    {}
//
//    /// return global pool for type T
//    static pool& global() { return SINGLETON(pool<T>); }
//
//    /// @brief Get item from pool
//    /// @return item from pool or nullptr
//    T* get_item() {
//        return stack_t::pop();
//    }
//
//    /// @brief Create instance or take one from pool
//    T* create_item() {
//        T* inst = stack_t::pop();
//        if (!inst)
//            inst = new T;
//        return inst;
//    }
//
//    /// return instance to pool
//    void release_item(T*& o) {
//        // create trait for reset !!!
//        stack_t::push(o);
//        o = 0;
//    }
//};

////////////////////////////////////////////////////////////////////////////////
///
//template<class T>
//class policy_pooled
//    : public policy_base
//{
//public:
//    COIDNEWDELETE(policy_pooled);
//
//    typedef policy_pooled<T> this_type;
//    typedef pool<this_type> pool_type;
//
//protected:
//    pool_type* _pool = 0;
//
//    T* _obj = 0;
//
//protected:
//
//    ///
//    explicit policy_pooled(T* const obj, pool_type* const p=0)
//        : policy_base()
//        , _pool(p)
//        , _obj(obj)
//    {}
//
//public:
//
//    T* get() const { return _obj; }
//
//    ///
//    virtual void _destroy() override
//    {
//        DASSERTN(_pool!=0);
//        _obj->reset();
//        this_type* t = static_cast<this_type*>(this);
//        _pool->release_item(t);
//    }
//
//    ///
//    static this_type* create(pool_type* po, bool nonew = false, bool* isnew = 0)
//    {
//        DASSERTN(po!=0);
//        this_type* p = po->get_item();
//
//        bool make = !p && !nonew;
//        if (make)
//            p = new this_type(new T, po);
//
//        if (isnew)
//            *isnew = make;
//
//        return p;
//    }
//
//    ///
//    static this_type* create( bool* isnew=0 )
//    {
//        return create(&default_pool(), false, isnew);
//    }
//
//    static pool_type& default_pool() { return pool_type::global(); }
//};
//
//////////////////////////////////////////////////////////////////////////////////
/////
//template<class T>
//class policy_pooled_i
//    : public policy_base
//{
//public:
//    COIDNEWDELETE(policy_pooled_i);
//
//    typedef policy_pooled_i<T> this_type;
//    typedef pool<this_type> pool_type;
//
//protected:
//    pool_type* _pool;
//
//public:
//
//    ///
//    policy_pooled_i()
//        : policy_base()
//        , _pool(&default_pool())
//    {}
//
//    ///
//    virtual void _destroy() override
//    {
//        DASSERTN(_pool!=0);
//        static_cast<T*>(this)->reset();
//        this_type* t = this;
//        _pool->release_item(t);
//    }
//
//    ///
//    static T* create(pool_type* po)
//    {
//        DASSERTN(po!=0);
//        this_type* p = 0;
//
//        if (!po->get_item(p)) {
//            p = new T;
//            p->_pool = po;
//        }
//
//        //if(init_counter)
//        p->add_refcount();
//
//        return static_cast<T*>(p);
//    }
//
//    ///
//    static T* create()
//    {
//        return create(&default_pool());
//    }
//
//    static pool_type& default_pool() { return pool_type::global(); }
//};

COID_NAMESPACE_END

#endif // __COMM_ATOMIC_POOL_BASE_H__
