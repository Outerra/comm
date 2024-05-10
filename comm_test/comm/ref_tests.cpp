#include <comm/ref_u.h>
#include <comm/ref_s.h>

#include <comm/ref/ref_policy_simple.h>
#include <comm/ref/ref_policy_pooled.h>

#include <comm/str.h>
#include <comm/commassert.h>
#include <comm/atomic/pool.h>



struct foo 
{
    inline static bool destructor_called = false;
    inline static const char* name = "foo";
    coid::charstr _member;

    foo() 
    {
        _member = name;
    }
    
    foo(coid::token value)
    {
        _member = value;
    }

    virtual ~foo()
    {
        foo::destructor_called = true;
    }
};

struct bar : public foo
{
    inline static bool destructor_called = false;
    inline static const char* name = "bar";

    coid::charstr _bar_member;
    
    bar() 
    {
        _member = name;
    }

    bar(int val)
    {
        _member = coid::charstr(val);
    }

    virtual ~bar()
    {
        bar::destructor_called = true;;
    }
};

SET_DEFAULT_REF_POLICY_TRAIT(bar, coid::ref_policy_pooled);

using foo_pool = coid::pool<foo>;
using bar_pool = coid::pool<bar>;

void reset()
{
    foo::destructor_called = false;
    bar::destructor_called = false;
    foo_pool::global().reset();
    bar_pool::global().reset();;
}


void ref_unique_tests()
{
    // ref_unique() test
    {
        uref<foo> foo_instance;
        DASSERT(foo_instance.is_empty());
    }

    // ref_unique(BaseOrDerivedType* object_ptr) test with base type + destructor test
    {
        reset();
        foo* ptr = new foo();
        uref<foo> foo_instance(ptr);
        DASSERT(foo_instance.is_set());
        foo_instance.release();
        DASSERT(foo_instance.is_empty());
        DASSERT(foo::destructor_called);
    }

    // ref_unique(BaseOrDerivedType* object_ptr) test with derived type + destructor test
    {
        reset();
        bar* ptr = new bar();
        uref<foo> foo_instance(ptr);
        DASSERT(foo_instance.is_set());
        foo_instance.release();
        DASSERT(foo_instance.is_empty());
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
    }

    // ref_unique(ref_unique<BaseOrDerivedType>&& rhs) move constructor test with base type
    {
        reset();
        foo* ptr = new foo();
        uref<foo> foo_instance(ptr);
        uref<foo> move_here = foo_instance.move();
        DASSERT(move_here.is_set());
        DASSERT(foo_instance.is_empty());
        DASSERT(foo::destructor_called == false);
    }

    // ref_unique(ref_unique<BaseOrDerivedType>&& rhs) move constructor test with derived type
    {
        reset();
        bar* ptr = new bar();
        uref<bar> bar_instance(ptr);
        uref<foo> move_here = bar_instance.move();
        DASSERT(move_here.is_set());
        DASSERT(bar_instance.is_empty());
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
    }

    // static ref_unique<Type> emplace(Args&&... args)  test
    {
        reset();
        uref<foo> foo_instance = uref<foo>::emplace("test_val");
        DASSERT(foo_instance.is_set());
        DASSERT(foo_instance->_member.cmpeq("test_val"));
    }

    // static ref_unique<Type> emplace(Args&&... args)  test
    {
        reset();
        uref<foo> foo_instance = uref<foo>::emplace<bar>(9);
        DASSERT(foo_instance.is_set());
        DASSERT(foo_instance->_member.cmpeq("9"));
    }

    // void create() test
    {
        reset();
        uref<foo> foo_instance;
        foo_instance.create();
        DASSERT(foo_instance.is_set());
        DASSERT(foo_instance->_member.cmpeq(foo::name));
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) test with global pool
    {
        uref<foo> foo_instance;
        foo_instance.create<coid::ref_policy_pooled<foo>>();
        foo_instance->_member = "foo from global pool";
        DASSERT(foo_instance.is_set());
        foo_instance.release();
        DASSERT(foo_instance.is_empty());
        foo* ptr = foo_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("foo from global pool"));
        DASSERT(foo_pool::global().get_item() == nullptr);
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) test with custom pool
    {
        foo_pool pool;
        uref<foo> foo_instance;
        foo_instance.create<coid::ref_policy_pooled<foo>>(&pool);
        foo_instance->_member = "from pool";
        DASSERT(foo_instance.is_set());
        foo_instance.release();
        DASSERT(foo_instance.is_empty());
        foo* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("from pool"));
        DASSERT(pool.get_item() == nullptr);
    }

    // template<typename DerivedType,typename Policy, typename... PolicyArguments void create(PolicyArguments&&... policy_arguments) test with global pool
    {
        uref<foo> foo_instance;
        foo_instance.create<bar, coid::ref_policy_pooled<bar>>();
        DASSERT(foo_instance->_member.cmpeq(bar::name));
        foo_instance->_member = "bar from global pool";
        DASSERT(foo_instance.is_set());
        foo_instance.release();
        DASSERT(foo_instance.is_empty());
        bar* ptr = bar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("bar from global pool"));
        DASSERT(bar_pool::global().get_item() == nullptr);
    }

    // template<typename DerivedType,typename Policy, typename... PolicyArguments>  void create(PolicyArguments&&... policy_arguments) test with custom pool
    {
        bar_pool pool;
        uref<foo> foo_instance;
        foo_instance.create<bar, coid::ref_policy_pooled<bar>>(&pool);
        DASSERT(foo_instance->_member.cmpeq(bar::name));
        foo_instance->_member = "from pool";
        DASSERT(foo_instance.is_set());
        foo_instance.release();
        DASSERT(foo_instance.is_empty());
        foo* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("from pool"));
        DASSERT(pool.get_item() == nullptr);
    }
}

void ref_shared_tests() 
{
    /// ref_shared() = default
    {
        reset();
        ref<foo> foo_instance;
        DASSERT(foo_instance.is_empty());
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> explicit ref_shared(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) test with base type
    {
        reset();
        ref<foo> instance(new foo);
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(instance->_member.cmpeq(foo::name));
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> explicit ref_shared(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) test with derived type with local pool
    {
        reset();
        bar_pool pool;
        ref<foo> instance(pool.create_item(), &pool);
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(instance->_member.cmpeq(bar::name));
        instance->_member = "bar from pool";
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(bar::destructor_called == false);
        bar* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(pool.get_item() == nullptr);
        DASSERT(ptr->_member.cmpeq("bar from pool"));
        delete ptr;
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> explicit ref_shared(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) test with derived type with global pool
    {
        reset();
        ref<foo> instance(new bar);
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(instance->_member.cmpeq(bar::name));
        instance->_member = "return to global pool";
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(bar_pool::global().get_item() == nullptr);
        DASSERT(ptr->_member.cmpeq("return to global pool"));
        delete ptr;
    }


    // template<typename BaseOrDerivedType> ref_shared(ref_unique<BaseOrDerivedType> && rhs) noexcept move base to base
    {
        reset();
        ref<foo> instance(new foo);
        ref<foo> move_here = instance.move();
        DASSERT(instance.is_empty());
        DASSERT(move_here.is_set());
        DASSERT(move_here.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        move_here.release();
        DASSERT(move_here.is_empty());
        DASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_shared(ref_unique<BaseOrDerivedType> && rhs) noexcept move derived to base
    {
        reset();
        ref<bar> instance(new bar);
        ref<foo> move_here = instance.move();
        DASSERT(instance.is_empty());
        DASSERT(move_here.is_set());
        DASSERT(move_here.get_strong_refcount() == 1);
        DASSERT(bar::destructor_called == false);
        move_here->_member = "return to global pool";
        move_here.release();
        DASSERT(move_here.is_empty());
        DASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("return to global pool"));
        DASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType> ref_shared(const ref_shared<BaseOrDerivedType>&rhs) copy base to base
    {
        reset();
        ref<foo> first(new foo);
        ref<foo> second(first);
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        DASSERT(foo::destructor_called == false);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(foo::destructor_called);
    }
}

void ref_tests() 
{
    ref_unique_tests();
    ref_shared_tests();
}
