#include <comm/ref_u.h>
#include <comm/ref_s.h>
#include <comm/ref_i.h>

#include <comm/ref/ref_policy_simple.h>
#include <comm/ref/ref_policy_pooled.h>

#include <comm/str.h>
#include <comm/commassert.h>
#include <comm/atomic/pool.h>

uint32 get_ref_intrusive_base_strong_refcount(coid::ref_intrusive_base* ptr)
{
    return (*reinterpret_cast<coid::ref_policy_base**>(reinterpret_cast<char*>(ptr) + sizeof(ptr)))->_counter.get_strong_counter_value();
}

struct boo 
{
    inline static bool destructor_called = false;
    inline static const char* name = "boo";

    int dummy_member = -8;

    virtual ~boo()
    {
        boo::destructor_called = true;
    }
};

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

struct bar : public boo, public foo
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
        bar::destructor_called = true;
    }
};

struct ifoo : public coid::ref_intrusive_base
{
    inline static bool destructor_called = false;
    inline static const char* name = "ifoo";

    coid::charstr _member;

    ifoo() 
    {
        _member = name;
    }

    virtual ~ifoo()
    {
        destructor_called = true;
    }
};

struct ibar : public boo, public ifoo
{
    inline static bool destructor_called = false;
    inline static const char* name = "ibar";

    ibar()
    {
        _member = name;
    }

    virtual ~ibar()
    {
        destructor_called = true;
    }
};

SET_DEFAULT_REF_POLICY_TRAIT(bar, coid::ref_policy_pooled);
SET_DEFAULT_REF_POLICY_TRAIT(ibar, coid::ref_policy_pooled);


using foo_pool = coid::pool<foo>;
using bar_pool = coid::pool<bar>;

using ifoo_pool = coid::pool<ifoo>;
using ibar_pool = coid::pool<ibar>;

void reset()
{
    foo_pool::global().reset();
    bar_pool::global().reset();
    ifoo_pool::global().reset();
    ibar_pool::global().reset();
    boo::destructor_called = false;
    foo::destructor_called = false;
    bar::destructor_called = false;
    ifoo::destructor_called = false;
    ibar::destructor_called = false;
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
        DASSERT(boo::destructor_called);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
    }

    // ref_unique(ref_unique<BaseOrDerivedType>&& rhs) move constructor test with base type
    {
        reset();
        foo* ptr = new foo();
        uref<foo> foo_instance(ptr);
        uref<foo> move_here(foo_instance.move());
        DASSERT(move_here.is_set());
        DASSERT(foo_instance.is_empty());
        DASSERT(foo::destructor_called == false);
        move_here.release();
        DASSERT(move_here.is_empty());
        DASSERT(foo::destructor_called);
    }

    // ref_unique(ref_unique<BaseOrDerivedType>&& rhs) move constructor test with derived type
    {
        reset();
        bar* ptr = new bar();
        uref<bar> bar_instance(ptr);
        uref<foo> move_here(bar_instance.move());
        DASSERT(move_here.is_set());
        DASSERT(bar_instance.is_empty());
        DASSERT(boo::destructor_called == false);
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        move_here.release();
        DASSERT(move_here.is_empty());
        DASSERT(boo::destructor_called);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
    }

    // ref_unique& operator=(ref_unique&& rhs) move assignment operator with base type
    {
        reset();
        uref<foo> foo_instance(new foo);
        uref<foo> move_here(new foo);
        move_here = foo_instance.move();
        DASSERT(foo::destructor_called);
        reset();
        DASSERT(move_here.is_set());
        DASSERT(foo_instance.is_empty());
        move_here.release();
        DASSERT(move_here.is_empty());
        DASSERT(foo::destructor_called);
    }

    // ref_unique& operator=(ref_unique&& rhs) move assigment operator with base type - assignment to self
    {
        reset();
        uref<foo> foo_instance(new foo);
        foo_instance = foo_instance.move();
        DASSERT(foo_instance.is_set());
        DASSERT(foo::destructor_called == false);
        foo_instance.release();
        DASSERT(foo_instance.is_empty());
        DASSERT(foo::destructor_called);
    }

    // ref_unique(ref_unique<BaseOrDerivedType>&& rhs) move assign operator test with derived type
    {
        reset();
        bar* ptr = new bar();
        uref<bar> bar_instance(ptr);
        uref<foo> move_here(new foo);
        move_here = bar_instance.move();
        DASSERT(move_here.is_set());
        DASSERT(bar_instance.is_empty());
        DASSERT(boo::destructor_called == false);
        DASSERT(foo::destructor_called == true);
        DASSERT(bar::destructor_called == false);
        reset();
        move_here.release();
        DASSERT(move_here.is_empty());
        DASSERT(boo::destructor_called);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
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
        reset();
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
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) test with custom pool
    {
        reset();
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
        delete ptr;
    }

    // template<typename DerivedType,typename Policy, typename... PolicyArguments void create(PolicyArguments&&... policy_arguments) test with global pool
    {
        reset();
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
        delete ptr;
    }

    // template<typename DerivedType,typename Policy, typename... PolicyArguments>  void create(PolicyArguments&&... policy_arguments) test with custom pool
    {
        reset();
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
        delete ptr;
    }


    // template<typename DerivedType,typename Policy, typename... PolicyArguments void create(PolicyArguments&&... policy_arguments) test with simple policy
    {
        reset();
        uref<foo> foo_instance;
        foo_instance.create<bar, coid::ref_policy_simple<bar>>();
        DASSERT(foo_instance->_member.cmpeq(bar::name));
        DASSERT(foo_instance.is_set());
        foo_instance.release();
        DASSERT(foo_instance.is_empty());
        DASSERT(boo::destructor_called);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
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


    // template<typename BaseOrDerivedType> ref_shared(ref_unique<BaseOrDerivedType> && rhs) noexcept move constuructor base to base
    {
        reset();
        ref<foo> instance(new foo);
        ref<foo> move_here(instance.move());
        DASSERT(instance.is_empty());
        DASSERT(move_here.is_set());
        DASSERT(move_here.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        move_here.release();
        DASSERT(move_here.is_empty());
        DASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_shared(ref_unique<BaseOrDerivedType> && rhs) noexcept move constructor derived to base
    {
        reset();
        ref<bar> instance(new bar);
        ref<foo> move_here(instance.move());
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

    // ref_shared(const ref_shared& rhs) base copy constructor 
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

    // ref_shared(const ref_shared<DerivedType>& rhs) derived to base copy constructor 
    {
        reset();
        ref<bar> first(new bar);
        first->_member = "return to global bar pool";
        ref<foo> second(first);
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        DASSERT(bar::destructor_called == false);
        first.release();
        DASSERT(bar::destructor_called == false);
        DASSERT(first.is_empty());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("return to global bar pool"));
        DASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // ref_shared& operator=(const ref_shared& rhs) assignment base to base 
    {
        reset();
        ref<foo> first(new foo);
        ref<foo> second(new foo);
        second = first;
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        DASSERT(foo::destructor_called);
        reset();
        first.release();
        DASSERT(first.is_empty());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(foo::destructor_called);
    }

    // ref_shared& operator=(const ref_shared& rhs) assignment self to self, base to base
    {
        reset();
        ref<foo> first(new foo);
        first = first;
        DASSERT(first.is_set());
        DASSERT(first.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(foo::destructor_called);
    }

    // ref_shared& operator=(const ref_shared<DerivedType>& rhs) assignment derived to base
    {
        reset();
        ref<bar> first(new bar);
        first->_member = "return to global bar pool";
        ref<foo> second(new foo);
        second = first;
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called == false);
        first.release();
        DASSERT(bar::destructor_called == false);
        DASSERT(first.is_empty());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("return to global bar pool"));
        DASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    //const ref_shared& operator=(const ref_shared<DerivedType>& rhs) assignment derived to base with simple policy
    {
        reset();
        ref<bar> first;
        first.create<coid::ref_policy_simple<bar>>();
        ref<foo> second;
        second = first;
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        DASSERT(boo::destructor_called == false);
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        first.release();
        DASSERT(boo::destructor_called == false);
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        DASSERT(first.is_empty());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(boo::destructor_called);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
    }

    // ref_shared& operator=(ref_shared&& rhs) move assignment base to base 
    {
        reset();
        ref<foo> first(new foo);
        ref<foo> second(new foo);
        second = first.move();
        DASSERT(first.is_empty());
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called);
        reset();
        second.release();
        DASSERT(second.is_empty());
        DASSERT(foo::destructor_called);
    }

    // ref_shared& operator=(ref_shared&& rhs) move assignment self to self, base to base
    {
        reset();
        ref<foo> first(new foo);
        first = first.move();
        DASSERT(first.is_set());
        DASSERT(first.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(foo::destructor_called);
    }

    // ref_shared& operator=(ref_shared<DerivedType>&& rhs) move assignment derived to base
    {
        reset();
        ref<bar> first(new bar);
        first->_member = "return to global bar pool";
        ref<foo> second(new foo);
        second = first.move();
        DASSERT(first.is_empty());
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("return to global bar pool"));
        DASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;        
    }

    // template<typename BaseOrDerivedType> bool operator==(const ref_intrusive<Type>& rhs) 
    {
        reset();
        ref<bar> first(new bar);
        ref<foo> second(first);
        DASSERT(first == first);
        DASSERT(first == second);
        DASSERT(second == first);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with base class
    {
        reset();
        ref<foo> r;
        r.create();
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class (global pool)
    {
        reset();
        ref<foo> r;
        r.create<bar>();
        r->_member = "return to global bar pool";
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(bar::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("return to global bar pool"));
        DASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class (local pool)
    {
        reset();
        bar_pool pool;
        ref<foo> r;
        r.create<bar>(&pool);
        r->_member = "return to local bar pool";
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(bar::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(bar::destructor_called == false);
        bar* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("return to local bar pool"));
        DASSERT(pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with pooled policy and global pool
    {
        reset();
        ref<foo> r;
        r.create<coid::ref_policy_pooled<foo>>();
        r->_member = "return to global foo pool";
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(foo::destructor_called == false);
        foo* ptr = foo_pool::global().get_item();
        DASSERT(ptr->_member.cmpeq("return to global foo pool"));
        DASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with pooled policy and local pool
    {
        reset();
        coid::pool<foo> local_pool;
        ref<foo> r;
        r.create<coid::ref_policy_pooled<foo>>(&local_pool);
        r->_member = "return to local foo pool";
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(foo::destructor_called == false);
        foo* ptr = local_pool.get_item();
        DASSERT(ptr->_member.cmpeq("return to local foo pool"));
        DASSERT(local_pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with simple policy and bar type
    {
        reset();
        ref<foo> r;
        r.create<bar, coid::ref_policy_simple<bar>>();
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(boo::destructor_called == false);
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(boo::destructor_called);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments) default policy trait with foo
    {
        reset();
        ref<foo> r;
        r.create(new foo);
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments) default policy trait with derived - global pool
    {
        reset();
        ref<foo> r;
        r.create(new bar);
        r->_member = "return to global bar pool";
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        foo* ptr = bar_pool::global().get_item();
        DASSERT(ptr->_member.cmpeq("return to global bar pool"));
        DASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename DerivedType, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments) default policy trait and derived type(pooled and bar) - local pool
    {
        reset();
        coid::pool<bar> local_pool;
        ref<foo> r;
        r.create(new bar, &local_pool);
        r->_member = "return to local bar pool";
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        foo* ptr = local_pool.get_item();
        DASSERT(ptr->_member.cmpeq("return to local bar pool"));
        DASSERT(local_pool.get_item() == nullptr);
        delete ptr;
    }


    // template<typename Policy, typename... PolicyArguments> void create(Type* object_ptr, PolicyArguments&&... policy_arguments) with pooled policy and global pool
    {
        reset();
        ref<foo> r;
        r.create<coid::ref_policy_pooled<foo>>(new foo);
        r->_member = "return to global foo pool";
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(foo::destructor_called == false);
        foo* ptr = foo_pool::global().get_item();
        DASSERT(ptr->_member.cmpeq("return to global foo pool"));
        DASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(Type* object_ptr, PolicyArguments&&... policy_arguments)  with pooled policy and local pool
    {
        reset();
        coid::pool<foo> local_pool;
        ref<foo> r;
        r.create<coid::ref_policy_pooled<foo>>(new foo, &local_pool);
        r->_member = "return to local foo pool";
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(foo::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(foo::destructor_called == false);
        foo* ptr = local_pool.get_item();
        DASSERT(ptr->_member.cmpeq("return to local foo pool"));
        DASSERT(local_pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments) with simple policy and bar type
    {
        reset();
        ref<foo> r;
        r.create<bar, coid::ref_policy_simple<bar>>(new bar);
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(boo::destructor_called == false);
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(boo::destructor_called);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments) with default policy(pooled) and bar type
    {
        reset();
        ref<foo> r;
        r.create<coid::ref_policy_simple<bar>>(new bar);
        DASSERT(r.is_set());
        DASSERT(r.get_strong_refcount() == 1);
        DASSERT(boo::destructor_called == false);
        DASSERT(foo::destructor_called == false);
        DASSERT(bar::destructor_called == false);
        r.release();
        DASSERT(r.is_empty());
        DASSERT(boo::destructor_called);
        DASSERT(foo::destructor_called);
        DASSERT(bar::destructor_called);
    }
}

void ref_intrusive_tests()
{
    // explicit ref_intrusive(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) with ifoo type
    {
        reset();
        iref<ifoo> instance(new ifoo);
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called);
    }

    // explicit ref_intrusive(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) with ibar type and global pool
    {
        reset();
        iref<ifoo> instance(new ibar);
        instance->_member = "returned to global pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ibar::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        DASSERT(ptr->_member.cmpeq("returned to global pool"));
        DASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // explicit ref_intrusive(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) with ibar type and local pool
    {
        reset();
        ibar_pool pool;
        iref<ifoo> instance(new ibar, &pool);
        instance->_member = "returned to local pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ibar::destructor_called == false);
        ibar* ptr = pool.get_item();
        DASSERT(ptr->_member.cmpeq("returned to local pool"));
        DASSERT(pool.get_item() == nullptr);
        delete ptr;
    }

    //  ref_intrusive(const ref_intrusive& rhs) copy consturctor base to base
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(first);
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(ifoo::destructor_called == false);
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive(const ref_intrusive<DerivedType>& rhs) copy consturctor derived to base
    {
        reset();
        iref<ibar> first(new ibar);
        first->_member = "returned to global pool";
        iref<ifoo> second(first);
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(ifoo::destructor_called == false);
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to global pool"));
        DASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    //  ref_intrusive(ref_intrusive&& rhs) move consturctor base to base
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(first.move());
        DASSERT(first.is_empty());
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive(ref_intrusive&& rhs) move consturctor derived to base
    {
        reset();
        iref<ibar> first(new ibar);
        first->_member = "returned to global pool";
        iref<ifoo> second(first.move());
        DASSERT(first.is_empty());
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to global pool"));
        DASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType> ref_intrusive& operator=(BaseOrDerivedType * rhs) assignment from pointer of base (simple policy)
    {
        reset();
        iref<ifoo> instance;
        instance = new ifoo;
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_intrusive& operator=(BaseOrDerivedType * rhs) assignment from pointer of derived (pooled policy - global)
    {
        reset();
        iref<ifoo> instance;
        instance = new ibar;
        instance->_member = "return to global pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ibar::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("return to global pool"));
        DASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    //  ref_intrusive& operator= (const ref_intrusive& rhs) assignment operator base to base
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second;
        second = first;
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(ifoo::destructor_called == false);
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive& operator= (const ref_intrusive& rhs) assignment operator base to base with same object_ptr
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(first);
        second = first;
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(ifoo::destructor_called == false);
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive& operator= (const ref_intrusive& rhs) assignment operator base self to self
    {
        reset();
        iref<ifoo> first(new ifoo);
        first = first;
        DASSERT(first.is_set());
        DASSERT(first.get_strong_refcount() == 1);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(ifoo::destructor_called);
    }


    //  ref_intrusive& operator= (const ref_intrusive<DerivedType>& rhs) assignment operator derived to base
    {
        reset();
        iref<ibar> first(new ibar);
        first->_member = "returned to global pool";
        iref<ifoo> second;
        second = first;
        DASSERT(first.is_set());
        DASSERT(second.is_set());
        DASSERT(first.get_strong_refcount() == 2);
        DASSERT(second.get_strong_refcount() == 2);
        first.release();
        DASSERT(first.is_empty());
        DASSERT(ifoo::destructor_called == false);
        DASSERT(second.is_set());
        DASSERT(second.get_strong_refcount() == 1);
        second.release();
        DASSERT(second.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to global pool"));
        DASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType> bool operator==(const ref_intrusive<Type>& rhs)
    {
        reset();
        iref<ibar> first(new ibar);
        iref<ifoo> second(first);
        DASSERT(first == first);
        DASSERT(first == second);
        DASSERT(second == first);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) void create() with base class
    {
        reset();
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create();
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class and global pool
    {
        reset();
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create<ibar>();
        instance->_member = "returned to global pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to global pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class and local pool
    {
        reset();
        ibar_pool pool;
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create<ibar>(&pool);
        instance->_member = "returned to local pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ibar* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to local pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) pooled policy with base class and global pool
    {
        reset();
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ifoo>>();
        instance->_member = "returned to global pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ifoo* ptr = ifoo_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to global pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) pooled policy with base class and local pool
    {
        reset();
        ifoo_pool pool;
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ifoo>>(&pool);
        instance->_member = "returned to local pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ifoo* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to local pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }

    //template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) simple policy with derived class
    {
        reset();
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create<ibar, coid::ref_policy_simple<ibar>>();
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == true);
    }


    // template<typename BaseOrDerivedType, typename... PolicyArguments> void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments) with base class
    {
        reset();
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create(new ifoo);
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class and global pool
    {
        reset();
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create(new ibar());
        instance->_member = "returned to global pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to global pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class and local pool
    {
        reset();
        ibar_pool pool;
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create(new ibar(), &pool);
        instance->_member = "returned to local pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ibar* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to local pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(Type* object_ptr, PolicyArguments&&... policy_arguments) pooled policy with base class and global pool
    {
        reset();
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ifoo>>(new ifoo);
        instance->_member = "returned to global pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ifoo* ptr = ifoo_pool::global().get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to global pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) pooled policy with base class and local pool
    {
        reset();
        ifoo_pool pool;
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ifoo>>(new ifoo, &pool);
        instance->_member = "returned to local pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == false);
        ifoo* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to local pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments) simple policy with derived class
    {
        reset();
        iref<ifoo> instance;
        DASSERT(instance.is_empty());
        instance.create<ibar, coid::ref_policy_simple<ibar>>(new ibar);
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ifoo::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ifoo::destructor_called == true);
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments)  explicit pooled policy with derived class and local pool
    {
        reset();
        ibar_pool pool;
        iref<ibar> instance;
        DASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ibar>>(new ibar, &pool);
        instance->_member = "returned to local pool";
        DASSERT(instance.is_set());
        DASSERT(instance.get_strong_refcount() == 1);
        DASSERT(ibar::destructor_called == false);
        instance.release();
        DASSERT(instance.is_empty());
        DASSERT(ibar::destructor_called == false);
        ibar* ptr = pool.get_item();
        DASSERT(ptr != nullptr);
        DASSERT(ptr->_member.cmpeq("returned to local pool"));
        DASSERT(get_ref_intrusive_base_strong_refcount(ptr) == 0);
        delete ptr;
    }
}
void ref_tests() 
{
    ref_unique_tests();
    ref_shared_tests();
    ref_intrusive_tests();
}
