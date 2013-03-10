#ifndef __COMM_ATOMIC_POOL_BASE_H__
#define __COMM_ATOMIC_POOL_BASE_H__

#include "../namespace.h"
#include "../alloc/commalloc.h"
#include "../ref_base.h"
#include "stack_base.h"

COID_NAMESPACE_BEGIN

///
template<class T>
class pool
    : protected atomic::stack_base<T>
{
public:
	pool()
        : atomic::stack_base<T>()
	{}

	/// return global pool for type T
	static pool& global() { return SINGLETON(pool<T>); }

	/// create instance or take one from pool
	bool create_instance(T& o) {
		return this->pop(o);
	}

    /// return isntance to pool
	void release_instance( T& o) {
		// create trait for reset !!!
		this->push(o);
	}
};

///
template<class T>
class policy_pooled
	: public policy_base
{
public:
    COIDNEWDELETE(policy_pooled);

    typedef policy_pooled<T> this_type;
	typedef pool<this_type*> pool_type;

protected:
    pool_type* _pool;

    T* _obj;

protected:

    ///
	explicit policy_pooled(T* const obj, pool_type* const p=0) 
		: policy_base()
		, _pool(p) 
        , _obj(obj)
    {}

public:

    ///
	virtual void destroy() 
    { 
        DASSERT(_pool!=0); 
        _obj->reset();
		this_type* t = static_cast<this_type*>(this);
        _pool->release_instance(t); 
    }

    ///
	static this_type* create( pool_type* po, bool nonew=false ) 
    { 
        DASSERT(po!=0); 
        this_type* p=0;

        if(!po->create_instance(p) && !nonew)
            p = new this_type(new T,po);
        return p;
    }

    ///
	static this_type* create()
    {
        return create(&pool_type::global());
    }

    T* get() const { return _obj; }
};

///
template<class T>
class policy_pooled_i
	: public policy_base
{
public:
    COIDNEWDELETE(policy_pooled_i);

    typedef policy_pooled<T> this_type;
	typedef pool<T*> pool_type;

protected:
    pool_type* _pool;

public:

    ///
	policy_pooled_i() 
		: policy_base()
        , _pool(&pool_type::global()) 
    {}

    ///
	virtual void destroy() 
    { 
        DASSERT(_pool!=0); 
        T* o=static_cast<T*>(this);
        o->reset();
        _pool->release_instance(o);
    }

    ///
	static T* create(pool_type* po,bool init_counter=false) 
    { 
        DASSERT(po!=0); 
        T* p;

        if( !po->create_instance(p) )
            p=new T();

		if(init_counter)
			p->add_ref_copy();

        return p;
    }

    ///
	static T* create(bool init_counter=false)
    {
        return create(&pool_type::global(),init_counter);
    }
};

COID_NAMESPACE_END

#endif // __COMM_ATOMIC_POOL_BASE_H__
