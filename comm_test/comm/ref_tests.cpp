#include <comm/ref_u.h>
#include <comm/ref_s.h>
#include <comm/ref_i.h>

#include <comm/ref/ref_policy_simple.h>
#include <comm/ref/ref_policy_pooled.h>

#include <comm/str.h>
#include <comm/commassert.h>
#include <comm/atomic/pool.h>

#include <utility>   // std::swap
#include <algorithm> // std::sort

/*
    These tests assert with RASSERT, not DASSERT, and that is deliberate.

    DASSERT compiles to nothing outside a debug build unless COID_DASSERT_LOG is defined, and
    nothing in this repository defines it. A release run of ref_tests() would therefore evaluate
    no condition at all - and, since plenty of the assertions below carry the side effect being
    tested, such as instance.eject(&policy_ptr), whole test bodies would simply not happen. The
    suite would pass by not running.

    RASSERT is live in every configuration, so these tests mean the same thing in Release and
    ReleaseLTCG as they do in Debug. binstring.cpp and regex.cpp already work this way; the rest
    of comm_test still uses DASSERT and is a no-op in release, which is worth fixing there too.
*/

/*
    ----------------------------------------------------------------------------------------------
    Memory layout of the fixture types
    ----------------------------------------------------------------------------------------------

    Most of what these tests guard is pointer adjustment, so the fixture is built so that a base
    subobject never sits at offset zero. Every type below lists a second base after a first one
    that is non-empty, which forces the compiler to place it at a non-zero offset - converting
    between the two then has to add or subtract that offset, and any code that launders the
    pointer through void* loses the adjustment silently.

    Offsets below are MSVC x64 (sizeof(void*) == 8, vptr first, 8 byte alignment) and are here to
    make the tests readable, not to be asserted on. The tests check that an offset is *non-zero*
    and that two pointers agree - never that an offset equals a particular number - so they stay
    valid on other ABIs where the numbers differ.

    Building blocks:
        sizeof(void*)                    8
        coid::charstr                    8   (a single pointer - see the note at the bottom)
        coid::ref_counter                8   (two uint32 atomics)
        coid::ref_policy_base           16   (vptr + ref_counter)
        coid::ref_intrusive_base        16   (vptr @0 + _policy_ptr @8)

    Non-intrusive hierarchy:

        boo                 size 16     foo                 size 16
        +0   vptr                       +0   vptr
        +8   _boo_member (int)         +8   _foo_member (charstr)

        bar : boo, foo      size 40     <- the workhorse for the multiple inheritance tests
        +0   [boo]      vptr
        +8        _boo_member
        +16  [foo]      vptr            <- foo subobject at +16, NOT at 0
        +24       _foo_member
        +32  _bar_member

        deep_mid : foo      size 24     deep_leaf : boo, deep_mid   size 48
        +0   [foo]  vptr                +0   [boo]          vptr
        +8         _foo_member          +8                  _boo_member
        +16  _mid_member                +16  [deep_mid]     vptr      <- and [foo] starts here too
                                        +24                 _foo_member
                                        +32                 _mid_member
                                        +40  _leaf_member

        deep_leaf is the three level case: the policy is created for deep_leaf, but a ref<foo>
        stores a pointer to the foo subobject at +16. Downcasting that to deep_mid must land on
        +16 as well; taking the policy's original pointer instead lands on +0 and every member
        read from there is 16 bytes off. bar alone cannot show this, because there the policy's
        type and the cast target are the same type.

    Intrusive hierarchy:

        ifoo : ref_intrusive_base   size 24     ibar : boo, ifoo        size 40
        +0   [ref_intrusive_base]  vptr         +0   [boo]              vptr
        +8                         _policy_ptr  +8                      _boo_member
        +16  _ifoo_member                            +16  [ifoo] == [ref_intrusive_base]  vptr
                                                +24                     _policy_ptr
                                                +32                     _ifoo_member

        For ibar the intrusive base - and with it _policy_ptr, the pointer to the policy that
        holds the refcount - sits at +16, so ref_intrusive has to adjust before it can reach it. A ref to a
        base and a ref to the derived type of the same object therefore hold *different* addresses
        while sharing one refcount; several tests assert exactly that.

    Member naming: every type's own payload is named after the type that declares it - _boo_member,
    _foo_member, _bar_member, _mid_member, _leaf_member, _ifoo_member - so an assertion always says
    which subobject it is reaching into, and no name is shared between the two hierarchies.
    ibar is the only type that declares no payload beyond what it inherits.

    A note on charstr: it is a single pointer, so reading _foo_member through a misadjusted pointer
    interprets whatever bytes are there as a pointer and dereferences them. That is why the tests
    for broken adjustment compare addresses instead of reading members - a wrong pointer crashes
    the process rather than failing an assert.
*/

/// Plain polymorphic type, never reference counted on its own. Used as the *first* base of bar,
/// deep_leaf and ibar purely to push the second base off offset zero.
/// Layout (16): +0 vptr, +8 _boo_member
struct boo
{
    inline static bool destructor_called = false;
    inline static const char* name = "boo";

    int _boo_member = -8;

    virtual ~boo()
    {
        boo::destructor_called = true;
    }
};

/// The base type most ref_shared / ref_unique tests are instantiated on. Default policy is
/// ref_policy_simple, so a foo created through a ref is new'd and deleted.
/// Layout (16): +0 vptr, +8 _foo_member
struct foo
{
    inline static bool destructor_called = false;
    inline static const char* name = "foo";
    coid::charstr _foo_member;

    foo()
    {
        _foo_member = name;
    }

    foo(coid::token value)
    {
        _foo_member = value;
    }

    virtual ~foo()
    {
        foo::destructor_called = true;
    }
};

/// Derived type used for every "derived to base" conversion. Registered as pooled further down,
/// so releasing a bar returns it to bar_pool instead of deleting it - that is how the tests get
/// hold of the true object address after a ref has let go of it.
/// Layout (40): +0 [boo] vptr, +8 _boo_member, +16 [foo] vptr, +24 _foo_member, +32 _bar_member
/// @note foo sits at +16, so bar* -> foo* shifts the pointer by 16 bytes.
struct bar : public boo, public foo
{
    inline static bool destructor_called = false;
    inline static const char* name = "bar";

    coid::charstr _bar_member;

    bar()
    {
        _foo_member = name;
    }

    bar(int val)
    {
        _foo_member = coid::charstr(val);
    }

    virtual ~bar()
    {
        bar::destructor_called = true;
    }
};

/// Middle type of the three level hierarchy. On its own it is a plain single-inheritance type -
/// foo sits at +0 - it only becomes interesting once deep_leaf pushes it to +16.
/// Layout (24): +0 [foo] vptr, +8 _foo_member, +16 _mid_member
struct deep_mid : public foo
{
    inline static bool destructor_called = false;
    inline static const char* name = "deep_mid";

    int _mid_member = 11;

    deep_mid()
    {
        _foo_member = name;
    }

    virtual ~deep_mid()
    {
        destructor_called = true;
    }
};

/// Most derived type of the three level hierarchy, and the only fixture type that can expose a
/// lost this-adjustment. A ref<foo> created for a deep_leaf stores +16 while the policy's original
/// pointer is +0, so any cast that goes through the policy's void* lands 16 bytes short - and
/// unlike bar, the cast target (deep_mid) is not the policy's own type, so the two cannot coincide.
/// Layout (48): +0 [boo] vptr, +8 _boo_member, +16 [deep_mid]==[foo] vptr, +24 _foo_member,
///              +32 _mid_member, +40 _leaf_member
/// @note deep_mid and foo start at the same offset (+16) - deep_mid's own foo base is at +0.
struct deep_leaf : public boo, public deep_mid
{
    inline static bool destructor_called = false;
    inline static const char* name = "deep_leaf";

    int _leaf_member = 12;

    deep_leaf()
    {
        _foo_member = name;
    }

    virtual ~deep_leaf()
    {
        destructor_called = true;
    }
};

/// The base type the ref_intrusive tests are instantiated on. The refcount is not in the ref, it
/// is in _policy_ptr inside the intrusive base carried by the object itself.
/// Layout (24): +0 [ref_intrusive_base] vptr, +8 _policy_ptr, +16 _ifoo_member
struct ifoo : public coid::ref_intrusive_base
{
    inline static bool destructor_called = false;
    inline static const char* name = "ifoo";

    coid::charstr _ifoo_member;

    ifoo() 
    {
        _ifoo_member = name;
    }

    virtual ~ifoo()
    {
        destructor_called = true;
    }
};

/// Intrusive derived type. Registered as pooled further down, so a released ibar goes back to
/// ibar_pool with its intrusive base still in place - which is what the recycling tests inspect.
/// Layout (40): +0 [boo] vptr, +8 _boo_member, +16 [ifoo]==[ref_intrusive_base] vptr,
///              +24 _policy_ptr, +32 _ifoo_member
/// @note The intrusive base - and with it the refcount - sits at +16, so ref_intrusive has to
///  adjust the pointer before it can touch the counter. iref<ifoo> and iref<ibar> pointing at the
///  same object therefore hold different addresses while sharing one refcount.
struct ibar : public boo, public ifoo
{
    inline static bool destructor_called = false;
    inline static const char* name = "ibar";

    ibar()
    {
        _ifoo_member = name;
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

using deep_leaf_pool = coid::pool<deep_leaf>;

void reset()
{
    foo_pool::global().reset();
    bar_pool::global().reset();
    ifoo_pool::global().reset();
    ibar_pool::global().reset();
    deep_leaf_pool::global().reset();
    boo::destructor_called = false;
    foo::destructor_called = false;
    bar::destructor_called = false;
    deep_mid::destructor_called = false;
    deep_leaf::destructor_called = false;
    ifoo::destructor_called = false;
    ibar::destructor_called = false;
}

// Move operations and noexcept. The move constructors only steal pointers, so they cannot throw
// and are marked noexcept - which is what makes a relocating container move refs rather than copy
// them, since it relocates with move-if-noexcept. The move assignments release first, and
// releasing the last reference returns the policy to its pool; the pool allocates a node when its
// node pool is empty, and comm's operator new throws on failure. So they are deliberately not
// noexcept, and neither is ref_shared's move constructor from a ref_unique, which has to create a
// policy the ref_unique never had.
static_assert(std::is_nothrow_move_constructible_v<ref<foo>>,     "ref_shared move ctor must stay noexcept");
static_assert(std::is_nothrow_move_constructible_v<uref<foo>>,    "ref_unique move ctor must stay noexcept");
static_assert(std::is_nothrow_move_constructible_v<iref<ifoo>>,   "ref_intrusive move ctor must stay noexcept");

static_assert(!std::is_nothrow_move_assignable_v<ref<foo>>,       "release() can throw, so move assignment must not claim noexcept");
static_assert(!std::is_nothrow_move_assignable_v<uref<foo>>,      "release() can throw, so move assignment must not claim noexcept");
static_assert(!std::is_nothrow_move_assignable_v<iref<ifoo>>,     "release() can throw, so move assignment must not claim noexcept");

static_assert(!std::is_nothrow_constructible_v<ref<foo>, uref<foo>&&>,
    "ref_shared from ref_unique creates a policy, which can allocate - must not claim noexcept");

void ref_unique_tests()
{
    // ref_unique() test
    {
        uref<foo> foo_instance;
        RASSERT(foo_instance.is_empty());
    }

    // ref_unique(BaseOrDerivedType* object_ptr) test with base type + destructor test
    {
        reset();
        foo* ptr = new foo();
        uref<foo> foo_instance(ptr);
        RASSERT(foo_instance.is_set());
        foo_instance.release();
        RASSERT(foo_instance.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_unique(BaseOrDerivedType* object_ptr) test with derived type + destructor test
    {
        reset();
        bar* ptr = new bar();
        uref<foo> foo_instance(ptr);
        RASSERT(foo_instance.is_set());
        foo_instance.release();
        RASSERT(foo_instance.is_empty());
        RASSERT(boo::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called);
    }

    // ref_unique(ref_unique<BaseOrDerivedType>&& rhs) move constructor test with base type
    {
        reset();
        foo* ptr = new foo();
        uref<foo> foo_instance(ptr);
        uref<foo> move_here(foo_instance.move());
        RASSERT(move_here.is_set());
        RASSERT(foo_instance.is_empty());
        RASSERT(foo::destructor_called == false);
        move_here.release();
        RASSERT(move_here.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_unique(ref_unique<BaseOrDerivedType>&& rhs) move constructor test with derived type
    {
        reset();
        bar* ptr = new bar();
        uref<bar> bar_instance(ptr);
        uref<foo> move_here(bar_instance.move());
        RASSERT(move_here.is_set());
        RASSERT(bar_instance.is_empty());
        RASSERT(boo::destructor_called == false);
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        move_here.release();
        RASSERT(move_here.is_empty());
        RASSERT(boo::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called);
    }

    // ref_unique& operator=(ref_unique&& rhs) move assignment operator with base type
    {
        reset();
        uref<foo> foo_instance(new foo);
        uref<foo> move_here(new foo);
        move_here = foo_instance.move();
        RASSERT(foo::destructor_called);
        reset();
        RASSERT(move_here.is_set());
        RASSERT(foo_instance.is_empty());
        move_here.release();
        RASSERT(move_here.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_unique& operator=(ref_unique&& rhs) move assigment operator with base type - assignment to self
    {
        reset();
        uref<foo> foo_instance(new foo);
        foo_instance = foo_instance.move();
        RASSERT(foo_instance.is_set());
        RASSERT(foo::destructor_called == false);
        foo_instance.release();
        RASSERT(foo_instance.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_unique(ref_unique<BaseOrDerivedType>&& rhs) move assign operator test with derived type
    {
        reset();
        bar* ptr = new bar();
        uref<bar> bar_instance(ptr);
        uref<foo> move_here(new foo);
        move_here = bar_instance.move();
        RASSERT(move_here.is_set());
        RASSERT(bar_instance.is_empty());
        RASSERT(boo::destructor_called == false);
        RASSERT(foo::destructor_called == true);
        RASSERT(bar::destructor_called == false);
        reset();
        move_here.release();
        RASSERT(move_here.is_empty());
        RASSERT(boo::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called);
    }

    // constructor arguments are passed by constructing the object at the call site, so the
    // allocation stays visible - there is no factory hiding a new
    {
        reset();
        uref<foo> foo_instance(new foo("test_val"));
        RASSERT(foo_instance.is_set());
        RASSERT(foo_instance->_foo_member.cmpeq("test_val"));
    }

    // the same for a derived type, through create()
    {
        reset();
        uref<foo> foo_instance;
        foo_instance.create(new bar(9));
        RASSERT(foo_instance.is_set());
        RASSERT(foo_instance->_foo_member.cmpeq("9"));
        RASSERT(foo_instance.get<bar>() != nullptr);
    }

    // void create() test
    {
        reset();
        uref<foo> foo_instance;
        foo_instance.create();
        RASSERT(foo_instance.is_set());
        RASSERT(foo_instance->_foo_member.cmpeq(foo::name));
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) test with global pool
    {
        reset();
        uref<foo> foo_instance;
        foo_instance.create<coid::ref_policy_pooled<foo>>();
        foo_instance->_foo_member = "foo from global pool";
        RASSERT(foo_instance.is_set());
        foo_instance.release();
        RASSERT(foo_instance.is_empty());
        foo* ptr = foo_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("foo from global pool"));
        RASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) test with custom pool
    {
        reset();
        foo_pool pool;
        uref<foo> foo_instance;
        foo_instance.create<coid::ref_policy_pooled<foo>>(&pool);
        foo_instance->_foo_member = "from pool";
        RASSERT(foo_instance.is_set());
        foo_instance.release();
        RASSERT(foo_instance.is_empty());
        foo* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("from pool"));
        RASSERT(pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType,typename Policy, typename... PolicyArguments void create(PolicyArguments&&... policy_arguments) test with global pool
    {
        reset();
        uref<foo> foo_instance;
        foo_instance.create<bar, coid::ref_policy_pooled<bar>>();
        RASSERT(foo_instance->_foo_member.cmpeq(bar::name));
        foo_instance->_foo_member = "bar from global pool";
        RASSERT(foo_instance.is_set());
        foo_instance.release();
        RASSERT(foo_instance.is_empty());
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("bar from global pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType,typename Policy, typename... PolicyArguments>  void create(PolicyArguments&&... policy_arguments) test with custom pool
    {
        reset();
        bar_pool pool;
        uref<foo> foo_instance;
        foo_instance.create<bar, coid::ref_policy_pooled<bar>>(&pool);
        RASSERT(foo_instance->_foo_member.cmpeq(bar::name));
        foo_instance->_foo_member = "from pool";
        RASSERT(foo_instance.is_set());
        foo_instance.release();
        RASSERT(foo_instance.is_empty());
        foo* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("from pool"));
        RASSERT(pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType,typename Policy, typename... PolicyArguments void create(PolicyArguments&&... policy_arguments) test with simple policy
    {
        reset();
        uref<foo> foo_instance;
        foo_instance.create<bar, coid::ref_policy_simple<bar>>();
        RASSERT(foo_instance->_foo_member.cmpeq(bar::name));
        RASSERT(foo_instance.is_set());
        foo_instance.release();
        RASSERT(foo_instance.is_empty());
        RASSERT(boo::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called);
    }

    // ref_unique(nullptr_t) test
    {
        reset();
        uref<foo> foo_instance(nullptr);
        RASSERT(foo_instance.is_empty());
        RASSERT(foo_instance.get() == nullptr);
    }

    // ~ref_unique() destroys the object on scope exit without an explicit release()
    {
        reset();
        {
            uref<foo> foo_instance(new foo);
            RASSERT(foo::destructor_called == false);
        }
        RASSERT(foo::destructor_called);
    }

    // ~ref_unique() returns a policy owned object on scope exit
    {
        reset();
        {
            uref<foo> foo_instance;
            foo_instance.create<coid::ref_policy_pooled<foo>>();
            foo_instance->_foo_member = "returned to global pool";
        }
        RASSERT(foo::destructor_called == false);
        foo* ptr = foo_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // explicit operator bool() test
    {
        reset();
        uref<foo> foo_instance;
        RASSERT(!static_cast<bool>(foo_instance));
        foo_instance.create();
        RASSERT(static_cast<bool>(foo_instance));
    }

    // Type& operator*() and Type* get() test
    {
        reset();
        foo* ptr = new foo();
        uref<foo> foo_instance(ptr);
        RASSERT(foo_instance.get() == ptr);
        RASSERT(&(*foo_instance) == ptr);
        RASSERT((*foo_instance)._foo_member.cmpeq(foo::name));
        RASSERT(foo_instance.get_ptr_ref() == ptr);
    }

    // int operator==(const Type* ptr) / int operator!=(const Type* ptr) test
    {
        reset();
        foo* ptr = new foo();
        uref<foo> foo_instance(ptr);
        RASSERT(foo_instance == ptr);
        RASSERT(!(foo_instance != ptr));
        RASSERT(foo_instance != nullptr);
    }

    // ref_unique& operator=(Type* object_ptr) test - the previously held object must be destroyed
    {
        reset();
        uref<foo> foo_instance(new foo);
        foo_instance = new foo;
        RASSERT(foo::destructor_called);
        reset();
        RASSERT(foo_instance.is_set());
        foo_instance.release();
        RASSERT(foo::destructor_called);
    }

    // Type* eject() test without a policy - the caller takes over the ownership
    {
        reset();
        foo* ptr = new foo();
        uref<foo> foo_instance(ptr);
        foo* ejected = foo_instance.eject();
        RASSERT(ejected == ptr);
        RASSERT(foo_instance.is_empty());
        RASSERT(foo::destructor_called == false);
        delete ejected;
        RASSERT(foo::destructor_called);
    }

    // create<Policy>() has to recover the policy's original pointer as Policy::element_type*
    // before storing it - see the matching ref_shared test for the full explanation
    {
        reset();
        foo* stored_ptr = nullptr;
        {
            uref<foo> instance;
            instance.create<coid::ref_policy_pooled<deep_leaf>>();
            stored_ptr = instance.get();
        }
        deep_leaf* real_ptr = deep_leaf_pool::global().get_item();
        RASSERT(real_ptr != nullptr);
        RASSERT(static_cast<void*>(real_ptr) != static_cast<void*>(static_cast<foo*>(real_ptr))); // sanity: offset is non-zero
        RASSERT(stored_ptr == static_cast<foo*>(real_ptr));
        delete real_ptr;
    }

    // template<typename BaseOrDerivedType> void create() over a ref that already holds an object
    // has to destroy the old one first, like the other two create() overloads do
    {
        reset();
        uref<foo> instance(new foo);
        instance.create<foo>();
        RASSERT(foo::destructor_called);        // the previously held object is gone
        reset();
        RASSERT(instance.is_set());             // and the new one is in place
        RASSERT(instance->_foo_member.cmpeq(foo::name));
        instance.release();
        RASSERT(foo::destructor_called);
    }

    // the same, re-creating as a derived type over an existing object
    /// @note create<bar>() is a plain new even though bar is registered pooled - ref_unique's
    ///  policy-less create() does not consult default_ref_policy_trait. Name the policy to pool.
    {
        reset();
        uref<foo> instance(new foo);
        instance.create<bar>();
        RASSERT(foo::destructor_called);
        reset();
        RASSERT(instance.is_set());
        RASSERT(instance->_foo_member.cmpeq(bar::name));
        instance.release();
        RASSERT(bar::destructor_called);
        RASSERT(bar_pool::global().get_item() == nullptr);
    }

    // create<BaseOrDerivedType>() over a ref that owns a policy has to go through that policy,
    // so a pooled object is returned to its pool rather than deleted
    {
        reset();
        uref<foo> instance;
        instance.create<coid::ref_policy_pooled<foo>>();
        instance->_foo_member = "returned to global pool";
        instance.create<foo>();
        RASSERT(foo::destructor_called == false);   // pooled, not deleted
        RASSERT(instance.is_set());
        foo* ptr = foo_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }
    // ref_unique has no swap of its own - std::swap does the job through the move operations
    {
        reset();
        foo* first_ptr = new foo();
        foo* second_ptr = new foo();
        uref<foo> first(first_ptr);
        uref<foo> second(second_ptr);
        std::swap(first, second);
        RASSERT(first.get() == second_ptr);
        RASSERT(second.get() == first_ptr);
        RASSERT(foo::destructor_called == false);
    }

    // the same with an empty ref on one side
    {
        reset();
        foo* ptr = new foo();
        uref<foo> first(ptr);
        uref<foo> second;
        std::swap(first, second);
        RASSERT(first.is_empty());
        RASSERT(second.get() == ptr);
        RASSERT(foo::destructor_called == false);
    }

    // template<typename DerivedType = Type> DerivedType* get() const
    {
        reset();
        bar* object_ptr = new bar();
        uref<foo> instance(object_ptr);
        RASSERT(instance.get() == static_cast<foo*>(object_ptr));   // default argument, unchanged
        RASSERT(instance.get<bar>() == object_ptr);
        RASSERT(static_cast<void*>(instance.get()) != static_cast<void*>(instance.get<bar>()));
        RASSERT(instance.get<bar>()->_foo_member.cmpeq(bar::name));
    }

    // get<DerivedType>() on an empty ref stays null
    {
        reset();
        uref<foo> instance;
        RASSERT(instance.get() == nullptr);
        RASSERT(instance.get<bar>() == nullptr);
    }

    // template<typename DerivedType> void create(DerivedType* object_ptr)
    // takes ownership of an already constructed object, destroying whatever was held before
    {
        reset();
        uref<foo> instance(new foo);
        foo* object_ptr = new foo();
        instance.create(object_ptr);
        RASSERT(foo::destructor_called);            // the previously held object is gone
        reset();
        RASSERT(instance.get() == object_ptr);
        instance.release();
        RASSERT(foo::destructor_called);
    }

    // the same with a derived pointer - bar sits at +16 from foo, so the stored pointer is adjusted
    {
        reset();
        bar* object_ptr = new bar();
        uref<foo> instance;
        instance.create(object_ptr);
        RASSERT(instance.get() == static_cast<foo*>(object_ptr));
        RASSERT(instance.get<bar>() == object_ptr);
        RASSERT(instance->_foo_member.cmpeq(bar::name));
        instance.release();
        RASSERT(bar::destructor_called);
    }

    // create() over a ref that owns a policy still goes through that policy
    {
        reset();
        uref<foo> instance;
        instance.create<coid::ref_policy_pooled<foo>>();
        instance->_foo_member = "returned to global pool";
        instance.create(new foo);
        RASSERT(foo::destructor_called == false);   // pooled, not deleted
        foo* ptr = foo_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename DerivedType> ref_unique& operator=(DerivedType* object_ptr)
    // now takes a derived pointer directly rather than relying on the conversion at the call site
    {
        reset();
        uref<foo> instance(new foo);
        bar* object_ptr = new bar();
        instance = object_ptr;
        RASSERT(foo::destructor_called);            // the previously held object is gone
        reset();
        RASSERT(instance.get() == static_cast<foo*>(object_ptr));
        RASSERT(instance.get<bar>() == object_ptr);
        instance.release();
        RASSERT(bar::destructor_called);
    }

    // eject() under multiple inheritance: the ref and the policy deliberately hold two different
    // addresses - the ref the adjusted foo*, the policy the true deep_leaf* - and on_destroy()
    // uses the policy's own typed member, never the void* it hands out through get_original_ptr()
    {
        reset();
        uref<foo> instance;
        instance.create<coid::ref_policy_pooled<deep_leaf>>();

        deep_leaf* real_ptr = instance.get<deep_leaf>();
        foo* adjusted_ptr = instance.get();
        RASSERT(static_cast<void*>(real_ptr) != static_cast<void*>(adjusted_ptr)); // sanity: offset is non-zero

        coid::ref_policy_base* policy_ptr = nullptr;
        foo* ejected_ptr = instance.eject(&policy_ptr);
        RASSERT(ejected_ptr == adjusted_ptr);                                   // ref hands out the adjusted one
        RASSERT(policy_ptr->get_original_ptr() == static_cast<void*>(real_ptr)); // policy kept the true one

        policy_ptr->on_destroy();
        RASSERT(deep_leaf::destructor_called == false);      // pooled, not destroyed
        deep_leaf* back_ptr = deep_leaf_pool::global().get_item();
        RASSERT(back_ptr == real_ptr);                       // the true object went back, not the subobject
        RASSERT(deep_leaf_pool::global().get_item() == nullptr);
        delete back_ptr;
        RASSERT(deep_leaf::destructor_called);
    }

    // the same without a policy - the caller gets the adjusted foo* and deletes through it, which
    // only lands on the right complete object because foo's destructor is virtual
    {
        reset();
        deep_leaf* real_ptr = new deep_leaf();
        uref<foo> instance;
        instance.create(real_ptr);
        foo* ejected_ptr = instance.eject();
        RASSERT(ejected_ptr == static_cast<foo*>(real_ptr));
        RASSERT(static_cast<void*>(ejected_ptr) != static_cast<void*>(real_ptr));
        RASSERT(instance.is_empty());
        RASSERT(deep_leaf::destructor_called == false);

        delete ejected_ptr;                                  // virtual dispatch to ~deep_leaf
        RASSERT(deep_leaf::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(boo::destructor_called);                     // every base ran, so it was the complete object
    }

    // a policy whose element_type is a BASE of what the object really is: the policy stores the
    // subobject address rather than the object's, and delete through it still reaches the complete
    // object - but only because the destructor is virtual
    {
        reset();
        deep_leaf* real_ptr = new deep_leaf();
        foo* as_foo = real_ptr;
        uref<foo> instance;
        instance.create<coid::ref_policy_simple<foo>>(as_foo);

        coid::ref_policy_base* policy_ptr = nullptr;
        instance.eject(&policy_ptr);
        RASSERT(policy_ptr->get_original_ptr() == static_cast<void*>(as_foo));   // the subobject
        RASSERT(policy_ptr->get_original_ptr() != static_cast<void*>(real_ptr)); // not the object

        policy_ptr->on_destroy();
        RASSERT(deep_leaf::destructor_called);               // virtual dtor found the complete object
        RASSERT(boo::destructor_called);
    }
    // Type* eject(ref_policy_base** policy_ptr_out) with no policy - the caller takes the object
    {
        reset();
        foo* object_ptr = new foo();
        uref<foo> instance(object_ptr);
        foo* ejected_ptr = instance.eject();
        RASSERT(ejected_ptr == object_ptr);
        RASSERT(instance.is_empty());
        RASSERT(foo::destructor_called == false);   // the ref did not destroy it
        delete ejected_ptr;
        RASSERT(foo::destructor_called);
    }

    // the out parameter is filled with nullptr when there is no policy
    {
        reset();
        uref<foo> instance(new foo);
        coid::ref_policy_base* policy_ptr = reinterpret_cast<coid::ref_policy_base*>(1);
        foo* ejected_ptr = instance.eject(&policy_ptr);
        RASSERT(policy_ptr == nullptr);
        RASSERT(instance.is_empty());
        delete ejected_ptr;
    }

    // with a policy, the caller takes both and finishes the handover through the policy, so a
    // pooled object goes back to its pool rather than being deleted
    {
        reset();
        uref<foo> instance;
        instance.create<coid::ref_policy_pooled<foo>>();
        instance->_foo_member = "returned to global pool";
        foo* object_ptr = instance.get();

        coid::ref_policy_base* policy_ptr = nullptr;
        foo* ejected_ptr = instance.eject(&policy_ptr);
        RASSERT(ejected_ptr == object_ptr);
        RASSERT(policy_ptr != nullptr);
        RASSERT(instance.is_empty());
        RASSERT(foo_pool::global().get_item() == nullptr);  // not back yet - the caller holds it

        policy_ptr->on_destroy();
        RASSERT(foo::destructor_called == false);           // pooled, not deleted
        foo* ptr = foo_pool::global().get_item();
        RASSERT(ptr == object_ptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // ejecting an empty ref is fine and answers nullptr
    {
        reset();
        uref<foo> instance;
        coid::ref_policy_base* policy_ptr = reinterpret_cast<coid::ref_policy_base*>(1);
        RASSERT(instance.eject() == nullptr);
        RASSERT(instance.eject(&policy_ptr) == nullptr);
        RASSERT(policy_ptr == nullptr);
    }

    // not covered here: eject() on a policy owning ref without the out parameter aborts by design,
    // so it cannot be exercised from inside the suite

    // bool operator ==(const Type*) const / bool operator !=(const Type*) const
    {
        reset();

        static_assert(std::is_same_v<decltype(std::declval<const uref<foo>&>() == std::declval<const foo*>()), bool>,
            "ref_unique::operator== must return bool, not int");
        static_assert(std::is_same_v<decltype(std::declval<const uref<foo>&>() != std::declval<const foo*>()), bool>,
            "ref_unique::operator!= must return bool, not int");

        foo unrelated;
        uref<foo> instance(new foo);
        const foo* true_ptr = instance.get();

        RASSERT(instance == true_ptr);
        RASSERT((instance != true_ptr) == false);
        RASSERT(instance != &unrelated);
        RASSERT((instance == &unrelated) == false);

        instance.release();
        RASSERT(instance == nullptr);
        RASSERT((instance != nullptr) == false);
    }

    // create() never attaches a policy and never consults default_ref_policy_trait - it is always
    // a plain new. A policy is attached only when one is named: create<Policy>(). See the note on
    // ref_unique::create().

    // create() on an unregistered type, as itself - ref_policy_simple would only delete what
    // release() deletes anyway, so no policy is attached and this stays one allocation
    {
        reset();
        uref<foo> instance;
        instance.create();
        RASSERT(instance.is_set());
        RASSERT(instance->_foo_member.cmpeq(foo::name));

        // no policy: eject() without the out parameter is legal, and the out parameter form
        // reports nothing to hand over
        coid::ref_policy_base* policy_ptr = reinterpret_cast<coid::ref_policy_base*>(1);
        foo* true_ptr = instance.get();
        RASSERT(instance.eject(&policy_ptr) == true_ptr);
        RASSERT(policy_ptr == nullptr);
        RASSERT(instance.is_empty());
        RASSERT(foo::destructor_called == false);
        delete true_ptr;
        RASSERT(foo::destructor_called);
    }

    // naming the type explicitly is the same case - still no policy
    {
        reset();
        uref<foo> instance;
        instance.create<foo>();
        RASSERT(instance.is_set());
        coid::ref_policy_base* policy_ptr = reinterpret_cast<coid::ref_policy_base*>(1);
        foo* true_ptr = instance.eject(&policy_ptr);
        RASSERT(policy_ptr == nullptr);
        delete true_ptr;
        RASSERT(foo::destructor_called);
    }

    // create(new Type) is still the policy-less, one allocation path
    {
        reset();
        uref<foo> instance;
        instance.create(new foo);
        RASSERT(instance.is_set());
        // no policy, so eject() without the out parameter is still legal here
        foo* true_ptr = instance.eject();
        RASSERT(true_ptr != nullptr);
        RASSERT(instance.is_empty());
        RASSERT(foo::destructor_called == false);
        delete true_ptr;
        RASSERT(foo::destructor_called);
    }

    // create(new DerivedType) on a registered type stays policy-less on purpose - it came from
    // new, so it is deleted, not pooled
    {
        reset();
        uref<foo> instance;
        instance.create(new bar);
        RASSERT(instance.is_set());
        instance.release();
        RASSERT(bar::destructor_called);
        RASSERT(bar_pool::global().get_item() == nullptr);
    }

    // ref_unique orders the same way, which is what lets a container of them be sorted
    {
        reset();
        uref<foo> first;
        first.create();
        uref<foo> second;
        second.create();

        RASSERT((first < first) == false);
        RASSERT((first < second) != (second < first));

        uref<foo> empty_instance;
        RASSERT(empty_instance < first);
        RASSERT((first < empty_instance) == false);

        // a dynarray of them sorts without any extra comparator. Seeded in the wrong order on
        // purpose, so the assertions below fail if std::sort did nothing at all.
        // decided once, before any move - re-testing get() after the first move would read a
        // ref that has already been emptied
        const bool first_is_lower = first.get() < second.get();
        const foo* lower_ptr = first_is_lower ? first.get() : second.get();
        const foo* higher_ptr = first_is_lower ? second.get() : first.get();

        coid::dynarray<uref<foo>> array;
        array.push(first_is_lower ? std::move(second) : std::move(first));
        array.push(first_is_lower ? std::move(first) : std::move(second));
        RASSERT(array.size() == 2);
        RASSERT(array[0].get() == higher_ptr);          // descending before the sort
        RASSERT(array[1].get() == lower_ptr);

        std::sort(array.ptr(), array.ptr() + array.size());

        RASSERT(array[0].get() == lower_ptr);           // ascending after it
        RASSERT(array[1].get() == higher_ptr);
        RASSERT(array[0] < array[1]);
        array.reset();
    }

    // Comparison against a raw pointer. Each ref declares one operator==(const Type*) and no
    // operator!=: C++20 synthesizes the negation and the reversed operand order from it, while
    // declaring != in the same scope would suppress both rewritings - MSVC C7692.

    // ref_unique against a raw pointer. The reversed form is the one its operator!= suppressed.
    {
        reset();
        uref<foo> instance;
        instance.create();
        foo* true_ptr = instance.get();

        RASSERT(instance == true_ptr);
        RASSERT(true_ptr == instance);
        RASSERT((instance != true_ptr) == false);
        RASSERT((true_ptr != instance) == false);

        instance.release();
        RASSERT(instance == nullptr);
        RASSERT(instance != true_ptr);
    }

    // The two halves of create(). The policy-less form never consults default_ref_policy_trait, so
    // what a call does is visible at the call site rather than depending on a trait registration in
    // some other header - and with it, whether eject() needs its out parameter.

    // create() on a type registered pooled - still a plain new, and deleted rather than recycled.
    // This is the behaviour change: the trait is not consulted here at all.
    {
        reset();
        uref<bar> instance;
        instance.create();
        RASSERT(instance.is_set());
        RASSERT(instance->_foo_member.cmpeq(bar::name));

        instance.release();
        RASSERT(bar::destructor_called);                    // deleted, not recycled
        RASSERT(bar_pool::global().get_item() == nullptr);  // the pool never saw it
    }

    // no policy means eject() needs no out parameter, whatever the type's trait says
    {
        reset();
        uref<bar> instance;
        instance.create();
        bar* true_ptr = instance.eject();
        RASSERT(true_ptr != nullptr);
        RASSERT(instance.is_empty());
        RASSERT(bar::destructor_called == false);
        delete true_ptr;
        RASSERT(bar::destructor_called);
    }

    // naming the policy is how pooling is asked for, and it still works
    {
        reset();
        uref<bar> instance;
        instance.create<coid::ref_policy_pooled<bar>>();
        RASSERT(instance.is_set());
        bar* true_ptr = instance.get();
        instance->_bar_member = "created through a named policy";

        instance.release();
        RASSERT(bar::destructor_called == false);
        bar* recycled = bar_pool::global().get_item();
        RASSERT(recycled == true_ptr);
        RASSERT(recycled->_bar_member.cmpeq("created through a named policy"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete recycled;
    }

    // and a named policy still takes its own arguments
    {
        reset();
        bar_pool local_pool;
        uref<bar> instance;
        instance.create<coid::ref_policy_pooled<bar>>(&local_pool);
        bar* true_ptr = instance.get();
        instance.release();
        RASSERT(bar::destructor_called == false);
        RASSERT(bar_pool::global().get_item() == nullptr);   // went back to the local pool
        bar* recycled = local_pool.get_item();
        RASSERT(recycled == true_ptr);
        delete recycled;
    }

    // create<DerivedType>() is a plain new too, so the stored pointer is the adjusted base one and
    // release() deletes through it - which is why Type needs a virtual destructor here
    {
        reset();
        uref<foo> instance;
        instance.create<deep_leaf>();
        RASSERT(instance.is_set());
        RASSERT(instance->_foo_member.cmpeq(deep_leaf::name));
        deep_leaf* true_ptr = instance.get<deep_leaf>();
        RASSERT(static_cast<const void*>(instance.get()) != static_cast<const void*>(true_ptr));

        coid::ref_policy_base* policy_ptr = reinterpret_cast<coid::ref_policy_base*>(1);
        RASSERT(instance.eject(&policy_ptr) == static_cast<foo*>(true_ptr));
        RASSERT(policy_ptr == nullptr);                     // no policy holds the true pointer
        delete static_cast<foo*>(true_ptr);                 // virtual destructor does the work
        RASSERT(deep_leaf::destructor_called);
        RASSERT(deep_mid::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(boo::destructor_called);
    }

    // a created uref moved into a ref_shared gets a ref_policy_simple, so a pooled-registered type
    // is deleted rather than recycled on that path - the consequence of not consulting the trait
    {
        reset();
        uref<bar> unique_instance;
        unique_instance.create();
        bar* true_ptr = unique_instance.get();

        ref<bar> shared_instance(unique_instance.move());
        RASSERT(unique_instance.is_empty());
        RASSERT(shared_instance.get() == true_ptr);
        RASSERT(shared_instance.get_strong_refcount() == 1);

        shared_instance.release();
        RASSERT(bar::destructor_called);
        RASSERT(bar_pool::global().get_item() == nullptr);
    }
}

void ref_shared_tests()
{
    /// ref_shared() = default
    {
        reset();
        ref<foo> foo_instance;
        RASSERT(foo_instance.is_empty());
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> explicit ref_shared(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) test with base type
    {
        reset();
        ref<foo> instance(new foo);
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(instance->_foo_member.cmpeq(foo::name));
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> explicit ref_shared(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) test with derived type with local pool
    {
        reset();
        bar_pool pool;
        ref<foo> instance(pool.create_item(), &pool);
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(instance->_foo_member.cmpeq(bar::name));
        instance->_foo_member = "bar from pool";
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(bar::destructor_called == false);
        bar* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(pool.get_item() == nullptr);
        RASSERT(ptr->_foo_member.cmpeq("bar from pool"));
        delete ptr;
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> explicit ref_shared(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) test with derived type with global pool
    {
        reset();
        ref<foo> instance(new bar);
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(instance->_foo_member.cmpeq(bar::name));
        instance->_foo_member = "return to global pool";
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(bar_pool::global().get_item() == nullptr);
        RASSERT(ptr->_foo_member.cmpeq("return to global pool"));
        delete ptr;
    }

    // template<typename BaseOrDerivedType> ref_shared(ref_shared<BaseOrDerivedType>&& rhs) noexcept move constuructor base to base
    {
        reset();
        ref<foo> instance(new foo);
        ref<foo> move_here(instance.move());
        RASSERT(instance.is_empty());
        RASSERT(move_here.is_set());
        RASSERT(move_here.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        move_here.release();
        RASSERT(move_here.is_empty());
        RASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_shared(ref_shared<BaseOrDerivedType>&& rhs) noexcept move constructor derived to base
    {
        reset();
        ref<bar> instance(new bar);
        ref<foo> move_here(instance.move());
        RASSERT(instance.is_empty());
        RASSERT(move_here.is_set());
        RASSERT(move_here.get_strong_refcount() == 1);
        RASSERT(bar::destructor_called == false);
        move_here->_foo_member = "return to global pool";
        move_here.release();
        RASSERT(move_here.is_empty());
        RASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("return to global pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // ref_shared(const ref_shared& rhs) base copy constructor 
    {
        reset();
        ref<foo> first(new foo);
        ref<foo> second(first);
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        RASSERT(foo::destructor_called == false);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_shared(const ref_shared<DerivedType>& rhs) derived to base copy constructor 
    {
        reset();
        ref<bar> first(new bar);
        first->_foo_member = "return to global bar pool";
        ref<foo> second(first);
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        RASSERT(bar::destructor_called == false);
        first.release();
        RASSERT(bar::destructor_called == false);
        RASSERT(first.is_empty());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("return to global bar pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // ref_shared& operator=(const ref_shared& rhs) assignment base to base 
    {
        reset();
        ref<foo> first(new foo);
        ref<foo> second(new foo);
        second = first;
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        RASSERT(foo::destructor_called);
        reset();
        first.release();
        RASSERT(first.is_empty());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_shared& operator=(const ref_shared& rhs) assignment self to self, base to base
    {
        reset();
        ref<foo> first(new foo);
        first = first;
        RASSERT(first.is_set());
        RASSERT(first.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_shared& operator=(const ref_shared<DerivedType>& rhs) assignment derived to base
    {
        reset();
        ref<bar> first(new bar);
        first->_foo_member = "return to global bar pool";
        ref<foo> second(new foo);
        second = first;
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called == false);
        first.release();
        RASSERT(bar::destructor_called == false);
        RASSERT(first.is_empty());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("return to global bar pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    //const ref_shared& operator=(const ref_shared<DerivedType>& rhs) assignment derived to base with simple policy
    {
        reset();
        ref<bar> first;
        first.create<coid::ref_policy_simple<bar>>();
        ref<foo> second;
        second = first;
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        RASSERT(boo::destructor_called == false);
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        first.release();
        RASSERT(boo::destructor_called == false);
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        RASSERT(first.is_empty());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(boo::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called);
    }

    // ref_shared& operator=(ref_shared&& rhs) move assignment base to base 
    {
        reset();
        ref<foo> first(new foo);
        ref<foo> second(new foo);
        second = first.move();
        RASSERT(first.is_empty());
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called);
        reset();
        second.release();
        RASSERT(second.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_shared& operator=(ref_shared&& rhs) move assignment self to self, base to base
    {
        reset();
        ref<foo> first(new foo);
        first = first.move();
        RASSERT(first.is_set());
        RASSERT(first.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(foo::destructor_called);
    }

    // ref_shared& operator=(ref_shared<DerivedType>&& rhs) move assignment derived to base
    {
        reset();
        ref<bar> first(new bar);
        first->_foo_member = "return to global bar pool";
        ref<foo> second(new foo);
        second = first.move();
        RASSERT(first.is_empty());
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("return to global bar pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;        
    }

    // template<typename BaseOrDerivedType> bool operator==(const ref_intrusive<Type>& rhs) 
    {
        reset();
        ref<bar> first(new bar);
        ref<foo> second(first);
        RASSERT(first == first);
        RASSERT(first == second);
        RASSERT(second == first);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with base class
    {
        reset();
        ref<foo> r;
        r.create();
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class (global pool)
    {
        reset();
        ref<foo> r;
        r.create<bar>();
        r->_foo_member = "return to global bar pool";
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(bar::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(bar::destructor_called == false);
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("return to global bar pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class (local pool)
    {
        reset();
        bar_pool pool;
        ref<foo> r;
        r.create<bar>(&pool);
        r->_foo_member = "return to local bar pool";
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(bar::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(bar::destructor_called == false);
        bar* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("return to local bar pool"));
        RASSERT(pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with pooled policy and global pool
    {
        reset();
        ref<foo> r;
        r.create<coid::ref_policy_pooled<foo>>();
        r->_foo_member = "return to global foo pool";
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(foo::destructor_called == false);
        foo* ptr = foo_pool::global().get_item();
        RASSERT(ptr->_foo_member.cmpeq("return to global foo pool"));
        RASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with pooled policy and local pool
    {
        reset();
        coid::pool<foo> local_pool;
        ref<foo> r;
        r.create<coid::ref_policy_pooled<foo>>(&local_pool);
        r->_foo_member = "return to local foo pool";
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(foo::destructor_called == false);
        foo* ptr = local_pool.get_item();
        RASSERT(ptr->_foo_member.cmpeq("return to local foo pool"));
        RASSERT(local_pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with simple policy and bar type
    {
        reset();
        ref<foo> r;
        r.create<bar, coid::ref_policy_simple<bar>>();
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(boo::destructor_called == false);
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(boo::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called);
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments) default policy trait with foo
    {
        reset();
        ref<foo> r;
        r.create(new foo);
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments) default policy trait with derived - global pool
    {
        reset();
        ref<foo> r;
        r.create(new bar);
        r->_foo_member = "return to global bar pool";
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        foo* ptr = bar_pool::global().get_item();
        RASSERT(ptr->_foo_member.cmpeq("return to global bar pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename DerivedType, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments) default policy trait and derived type(pooled and bar) - local pool
    {
        reset();
        coid::pool<bar> local_pool;
        ref<foo> r;
        r.create(new bar, &local_pool);
        r->_foo_member = "return to local bar pool";
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        foo* ptr = local_pool.get_item();
        RASSERT(ptr->_foo_member.cmpeq("return to local bar pool"));
        RASSERT(local_pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(Type* object_ptr, PolicyArguments&&... policy_arguments) with pooled policy and global pool
    {
        reset();
        ref<foo> r;
        r.create<coid::ref_policy_pooled<foo>>(new foo);
        r->_foo_member = "return to global foo pool";
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(foo::destructor_called == false);
        foo* ptr = foo_pool::global().get_item();
        RASSERT(ptr->_foo_member.cmpeq("return to global foo pool"));
        RASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(Type* object_ptr, PolicyArguments&&... policy_arguments)  with pooled policy and local pool
    {
        reset();
        coid::pool<foo> local_pool;
        ref<foo> r;
        r.create<coid::ref_policy_pooled<foo>>(new foo, &local_pool);
        r->_foo_member = "return to local foo pool";
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(foo::destructor_called == false);
        foo* ptr = local_pool.get_item();
        RASSERT(ptr->_foo_member.cmpeq("return to local foo pool"));
        RASSERT(local_pool.get_item() == nullptr);
        delete ptr;
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments) with simple policy and bar type
    {
        reset();
        ref<foo> r;
        r.create<bar, coid::ref_policy_simple<bar>>(new bar);
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(boo::destructor_called == false);
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(boo::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called);
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments)
    // @note the object pointer is deduced into the policy argument pack here, so this goes through
    //  the pack overload rather than the create(Type*) one, and the ref has to recover the pointer
    //  from the policy - bar sits at +16 from foo, so the adjustment is what is being checked
    {
        reset();
        bar* object_ptr = new bar;
        ref<foo> r;
        r.create<coid::ref_policy_simple<bar>>(object_ptr);
        RASSERT(r.is_set());
        RASSERT(r.get_strong_refcount() == 1);
        RASSERT(r.get() == static_cast<foo*>(object_ptr));
        RASSERT(r->_foo_member.cmpeq(bar::name));
        RASSERT(boo::destructor_called == false);
        RASSERT(foo::destructor_called == false);
        RASSERT(bar::destructor_called == false);
        r.release();
        RASSERT(r.is_empty());
        RASSERT(boo::destructor_called);
        RASSERT(foo::destructor_called);
        RASSERT(bar::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_shared(ref_unique<BaseOrDerivedType>&& rhs) noexcept
    // move constructor from a policy-less ref_unique - ref_policy_simple has to be substituted
    {
        reset();
        foo* ptr = new foo();
        uref<foo> unique_instance(ptr);
        ref<foo> shared_instance(unique_instance.move());
        RASSERT(unique_instance.is_empty());
        RASSERT(shared_instance.is_set());
        RASSERT(shared_instance.get() == ptr);
        RASSERT(shared_instance.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        shared_instance.release();
        RASSERT(shared_instance.is_empty());
        RASSERT(foo::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_shared(ref_unique<BaseOrDerivedType>&& rhs) noexcept
    // move constructor from a ref_unique that already owns a policy - the policy has to be taken over
    {
        reset();
        uref<foo> unique_instance;
        unique_instance.create<coid::ref_policy_pooled<foo>>();
        unique_instance->_foo_member = "returned to global pool";
        ref<foo> shared_instance(unique_instance.move());
        RASSERT(unique_instance.is_empty());
        RASSERT(shared_instance.is_set());
        RASSERT(shared_instance.get_strong_refcount() == 1);
        shared_instance.release();
        RASSERT(shared_instance.is_empty());
        RASSERT(foo::destructor_called == false);
        foo* ptr = foo_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(foo_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType> ref_shared(ref_unique<BaseOrDerivedType>&& rhs) noexcept
    // move constructor derived to base
    {
        reset();
        bar* ptr = new bar();
        uref<bar> unique_instance(ptr);
        ref<foo> shared_instance(unique_instance.move());
        RASSERT(unique_instance.is_empty());
        RASSERT(shared_instance.is_set());
        RASSERT(shared_instance.get() == static_cast<foo*>(ptr));
        RASSERT(shared_instance.get_strong_refcount() == 1);
        RASSERT(shared_instance->_foo_member.cmpeq(bar::name));
        shared_instance.release();
        RASSERT(bar::destructor_called);
    }

    // ref_shared(nullptr_t) test
    {
        reset();
        ref<foo> instance(nullptr);
        RASSERT(instance.is_empty());
        RASSERT(instance.get() == nullptr);
    }

    // ~ref_shared() releases on scope exit without an explicit release()
    {
        reset();
        {
            ref<foo> first(new foo);
            {
                ref<foo> second(first);
                RASSERT(first.get_strong_refcount() == 2);
            }
            RASSERT(first.get_strong_refcount() == 1);
            RASSERT(foo::destructor_called == false);
        }
        RASSERT(foo::destructor_called);
    }

    // explicit operator bool() test
    {
        reset();
        ref<foo> instance;
        RASSERT(!static_cast<bool>(instance));
        instance.create();
        RASSERT(static_cast<bool>(instance));
    }

    // Type& operator*() and Type* get() test
    {
        reset();
        foo* ptr = new foo();
        ref<foo> instance(ptr);
        RASSERT(instance.get() == ptr);
        RASSERT(&(*instance) == ptr);
        RASSERT((*instance)._foo_member.cmpeq(foo::name));
    }

    // copy constructor and copy assignment from an empty ref
    {
        reset();
        ref<foo> empty_instance;
        ref<foo> copy(empty_instance);
        RASSERT(copy.is_empty());

        ref<foo> assigned(new foo);
        assigned = empty_instance;
        RASSERT(assigned.is_empty());
        RASSERT(foo::destructor_called);
    }

    // create() on an already set ref has to release the previously held object
    {
        reset();
        ref<foo> instance(new foo);
        instance.create();
        RASSERT(foo::destructor_called);
        reset();
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
    }

    // template<typename DerivedType> ref_shared<DerivedType> downcast() const
    {
        reset();
        ref<foo> instance;
        instance.create<bar>();
        ref<bar> downcasted = instance.downcast<bar>();
        RASSERT(downcasted.is_set());
        RASSERT(static_cast<foo*>(downcasted.get()) == instance.get());
        RASSERT(downcasted->_foo_member.cmpeq(bar::name));
        RASSERT(instance.get_strong_refcount() == 2);
        instance.release();
        RASSERT(downcasted.get_strong_refcount() == 1);
        downcasted->_foo_member = "returned to global pool";
        downcasted.release();
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename DerivedType> ref_shared<DerivedType> downcast() const, to a type that is
    // not the policy's own type - the cast has to adjust the pointer, and taking the policy's
    // untyped original pointer instead lands on the wrong subobject.
    // deep_leaf derives from boo first and deep_mid second, so deep_mid sits at +16 inside it
    // while the policy's original pointer is the deep_leaf* at +0.
    {
        reset();
        ref<foo> instance;
        instance.create<deep_leaf>();

        // downcast to the policy's own type gives us the real object address to compare against
        deep_leaf* leaf_ptr = instance.downcast<deep_leaf>().get();
        RASSERT(leaf_ptr != nullptr);
        RASSERT(static_cast<void*>(leaf_ptr) != static_cast<void*>(static_cast<deep_mid*>(leaf_ptr))); // sanity: offset is non-zero

        ref<deep_mid> downcasted = instance.downcast<deep_mid>();
        RASSERT(downcasted.is_set());
        RASSERT(downcasted.get() == static_cast<deep_mid*>(leaf_ptr));
        RASSERT(downcasted->_mid_member == 11);
        RASSERT(downcasted->_foo_member.cmpeq(deep_leaf::name));
        RASSERT(instance.get_strong_refcount() == 2); // instance + downcasted, the deep_leaf temporary is gone

        instance.release();
        downcasted.release();
        RASSERT(deep_leaf::destructor_called);
    }

    // downcast() of an empty ref must stay empty instead of dereferencing the policy
    {
        reset();
        ref<foo> instance;
        ref<bar> downcasted = instance.downcast<bar>();
        RASSERT(downcasted.is_empty());
        RASSERT(instance.is_empty());
    }

    // create<Policy>() has to recover the policy's original pointer as Policy::element_type*
    // before storing it - casting the raw void* to Type* keeps the address of the most derived
    // object and points into the wrong subobject whenever element_type is not Type.
    // @note The true object address is taken from the pool after release, so the check does not
    //  lean on any other code path of the ref under test. Pointers are only compared, never
    //  dereferenced - reading a member through a misadjusted pointer crashes instead of asserting.
    {
        reset();
        foo* stored_ptr = nullptr;
        {
            ref<foo> instance;
            instance.create<coid::ref_policy_pooled<deep_leaf>>();
            RASSERT(instance.get_strong_refcount() == 1);
            stored_ptr = instance.get();
        }
        deep_leaf* real_ptr = deep_leaf_pool::global().get_item();
        RASSERT(real_ptr != nullptr);
        RASSERT(static_cast<void*>(real_ptr) != static_cast<void*>(static_cast<foo*>(real_ptr))); // sanity: offset is non-zero
        RASSERT(stored_ptr == static_cast<foo*>(real_ptr));
        delete real_ptr;
    }

    // create<Policy>() reaching the object through the ref must land on the right subobject -
    // this reads a member, which is only safe once the pointer is adjusted
    {
        reset();
        ref<foo> instance;
        instance.create<coid::ref_policy_simple<deep_leaf>>();
        RASSERT(instance->_foo_member.cmpeq(deep_leaf::name));
        instance.release();
        RASSERT(deep_leaf::destructor_called);
    }
    // get_strong_refcount() answers 0 for a ref that holds nothing, rather than dereferencing the
    // policy it does not have
    {
        reset();
        ref<foo> empty_instance;
        RASSERT(empty_instance.is_empty());
        RASSERT(empty_instance.get_strong_refcount() == 0);

        ref<foo> instance(new foo);
        RASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        RASSERT(instance.get_strong_refcount() == 0);       // released is empty again

        ref<foo> source(new foo);
        ref<foo> target(source.move());
        RASSERT(source.get_strong_refcount() == 0);         // and so is moved from
        RASSERT(target.get_strong_refcount() == 1);
    }

    // ref_shared must refuse intrusively counted types outright - it keeps its own policy and
    // never touches the object's intrusive refcount, so an iref adopting the same object would
    // become a second, independent owner and one of them would destroy it under the other.
    // ref<ifoo> does not compile; see the static_assert in ref_shared::assert_not_intrusive().
    // ref_shared has no swap of its own - std::swap does the job through the move operations,
    // and must not disturb the refcounts on the way
    {
        reset();
        foo* first_ptr = new foo();
        foo* second_ptr = new foo();
        ref<foo> first(first_ptr);
        ref<foo> second(second_ptr);
        std::swap(first, second);
        RASSERT(first.get() == second_ptr);
        RASSERT(second.get() == first_ptr);
        RASSERT(first.get_strong_refcount() == 1);
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
    }

    // the same with an empty ref on one side
    {
        reset();
        foo* ptr = new foo();
        ref<foo> first(ptr);
        ref<foo> second;
        std::swap(first, second);
        RASSERT(first.is_empty());
        RASSERT(second.get() == ptr);
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
    }

    // template<typename DerivedType = Type> DerivedType* get() const
    // bar derives from boo first and foo second, so viewing a bar through a ref<foo> and asking
    // for the bar back has to undo the +16 adjustment
    {
        reset();
        bar* object_ptr = new bar();
        ref<foo> instance(object_ptr);
        RASSERT(instance.get() == static_cast<foo*>(object_ptr));   // default argument, unchanged
        RASSERT(instance.get<bar>() == object_ptr);
        RASSERT(static_cast<void*>(instance.get()) != static_cast<void*>(instance.get<bar>()));
        RASSERT(instance.get<bar>()->_bar_member.is_empty());
        RASSERT(instance.get<foo>() == instance.get());
        instance->_foo_member = "returned to global pool";
        instance.release();
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr == object_ptr);
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // get<DerivedType>() on an empty ref stays null
    {
        reset();
        ref<foo> instance;
        RASSERT(instance.get() == nullptr);
        RASSERT(instance.get<bar>() == nullptr);
    }

    // The copy paths increment the strong counter plainly.
    //
    // They used to go through try_increase_strong_counter() and only take the pointers when the
    // CAS succeeded. The CAS can never fail here - the source holds a strong reference for the
    // whole call - but on a hypothetical failure the copy silently came out *empty* instead of
    // being a copy, which is the one outcome nothing downstream checks for. These tests pin the
    // exact counts and the pointer adjustment on all four copy paths.

    // ref_shared(const ref_shared& rhs) - same type copy constructor, exact refcount
    {
        reset();
        ref<foo> first(new foo);
        foo* true_ptr = first.get();
        ref<foo> second(first);
        ref<foo> third(second);
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(third.is_set());
        RASSERT(second.get() == true_ptr);
        RASSERT(third.get() == true_ptr);
        RASSERT(first.get_strong_refcount() == 3);
        third.release();
        RASSERT(first.get_strong_refcount() == 2);
        second.release();
        RASSERT(first.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        first.release();
        RASSERT(foo::destructor_called);
    }

    // template<typename DerivedType> ref_shared(const ref_shared<DerivedType>& rhs) - the derived
    // to base copy constructor still adjusts the pointer, and the shared policy keeps the true one
    {
        reset();
        ref<bar> derived(new bar);
        bar* true_ptr = derived.get();
        ref<foo> base(derived);
        RASSERT(base.is_set());
        RASSERT(derived.get_strong_refcount() == 2);
        RASSERT(base.get_strong_refcount() == 2);
        RASSERT(base.get() == static_cast<foo*>(true_ptr));
        // foo is not the first base of bar, so the copy had to shift the pointer
        RASSERT(static_cast<const void*>(base.get()) != static_cast<const void*>(true_ptr));
        base.release();
        RASSERT(derived.get_strong_refcount() == 1);
        derived.release();
        RASSERT(bar::destructor_called == false);
        bar* recycled = bar_pool::global().get_item();
        RASSERT(recycled == true_ptr);
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete recycled;
    }

    // ref_shared& operator=(const ref_shared& rhs) - same type copy assignment releases what the
    // target held before it counts the new reference
    {
        reset();
        ref<foo> source(new foo);
        foo* true_ptr = source.get();
        ref<foo> target(new bar);

        target = source;

        RASSERT(target.is_set());
        RASSERT(target.get() == true_ptr);
        RASSERT(source.get_strong_refcount() == 2);
        // the bar the target used to hold went back to its pool
        RASSERT(bar::destructor_called == false);
        bar* recycled = bar_pool::global().get_item();
        RASSERT(recycled != nullptr);
        RASSERT(bar_pool::global().get_item() == nullptr);

        source.release();
        RASSERT(target.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        target.release();
        RASSERT(foo::destructor_called);

        // deleting the recycled bar runs ~foo too, so it has to come after the checks above
        delete recycled;
    }

    // template<typename DerivedType> ref_shared& operator=(const ref_shared<DerivedType>& rhs)
    {
        reset();
        ref<bar> derived(new bar);
        bar* true_ptr = derived.get();
        ref<foo> base;

        base = derived;

        RASSERT(base.is_set());
        RASSERT(base.get() == static_cast<foo*>(true_ptr));
        RASSERT(derived.get_strong_refcount() == 2);
        derived.release();
        RASSERT(base.get_strong_refcount() == 1);
        base.release();
        bar* recycled = bar_pool::global().get_item();
        RASSERT(recycled == true_ptr);
        delete recycled;
    }

    // ref_shared& operator=(const ref_shared& rhs) - self assignment neither releases nor counts
    {
        reset();
        ref<foo> instance(new foo);
        foo* true_ptr = instance.get();
        const ref<foo>& alias = instance;

        instance = alias;

        RASSERT(instance.is_set());
        RASSERT(instance.get() == true_ptr);
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(foo::destructor_called == false);
        instance.release();
        RASSERT(foo::destructor_called);
    }

    // Comparison against nullptr, on all three refs. operator!= and the reversed operand
    // order are both synthesized by C++20 from operator==(nullptr_t).
    // ref_shared / ref_intrusive against nullptr, both operand orders
    {
        reset();
        ref<foo> empty_shared;
        RASSERT(empty_shared == nullptr);
        RASSERT((empty_shared != nullptr) == false);
        RASSERT(nullptr == empty_shared);              // C++20 reverses the operands for us
        RASSERT((nullptr != empty_shared) == false);

        ref<foo> set_shared(new foo);
        RASSERT(set_shared != nullptr);
        RASSERT((set_shared == nullptr) == false);
        set_shared.release();
        RASSERT(set_shared == nullptr);
        RASSERT(foo::destructor_called);

        iref<ifoo> empty_intrusive;
        RASSERT(empty_intrusive == nullptr);
        RASSERT(nullptr == empty_intrusive);

        iref<ifoo> set_intrusive;
        set_intrusive.create();
        RASSERT(set_intrusive != nullptr);
        set_intrusive.release();
        RASSERT(set_intrusive == nullptr);

        uref<foo> empty_unique;
        RASSERT(empty_unique == nullptr);
        RASSERT(nullptr == empty_unique);
        RASSERT((empty_unique != nullptr) == false);

        uref<foo> set_unique;
        set_unique.create();
        RASSERT(set_unique != nullptr);
        // the pointer overload still works alongside it, and agrees
        RASSERT(set_unique == set_unique.get());
        set_unique.release();
        RASSERT(set_unique == nullptr);
    }

    // operator< is a strict total order over the object addresses
    {
        reset();
        ref<foo> first(new foo);
        ref<foo> second(new foo);

        RASSERT((first < first) == false);                      // irreflexive
        RASSERT((first < second) != (second < first));          // exactly one direction holds

        ref<foo> first_copy(first);                             // a copy sits at the same place
        RASSERT((first < first_copy) == false);
        RASSERT((first_copy < first) == false);
        RASSERT(first == first_copy);

        ref<foo> empty_instance;                                // an empty ref sorts first
        RASSERT(empty_instance < first);
        RASSERT((first < empty_instance) == false);

        first_copy.release();
        first.release();
        second.release();
    }

    // Comparison against a raw pointer. Each ref declares one operator==(const Type*) and no
    // operator!=: C++20 synthesizes the negation and the reversed operand order from it, while
    // declaring != in the same scope would suppress both rewritings - MSVC C7692.

    // ref_shared against a raw pointer, all four spellings
    {
        reset();
        ref<foo> instance(new foo);
        foo* true_ptr = instance.get();
        foo unrelated;

        RASSERT(instance == true_ptr);
        RASSERT(true_ptr == instance);                      // reversed, synthesized
        RASSERT((instance != true_ptr) == false);
        RASSERT((true_ptr != instance) == false);

        RASSERT(instance != &unrelated);
        RASSERT(&unrelated != instance);
        RASSERT((instance == &unrelated) == false);

        instance.release();
        RASSERT(instance == nullptr);
        RASSERT(instance == static_cast<foo*>(nullptr));
        RASSERT(instance != true_ptr);
    }
}

void ref_intrusive_tests()
{
    // explicit ref_intrusive(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) with ifoo type
    {
        reset();
        iref<ifoo> instance(new ifoo);
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    // explicit ref_intrusive(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) with ibar type and global pool
    {
        reset();
        iref<ifoo> instance(new ibar);
        instance->_ifoo_member = "returned to global pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ibar::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // explicit ref_intrusive(BaseOrDerivedType * object_ptr, PolicyArguments&&... policy_arguments) with ibar type and local pool
    {
        reset();
        ibar_pool pool;
        iref<ifoo> instance(new ibar, &pool);
        instance->_ifoo_member = "returned to local pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ibar::destructor_called == false);
        ibar* ptr = pool.get_item();
        RASSERT(ptr->_ifoo_member.cmpeq("returned to local pool"));
        RASSERT(pool.get_item() == nullptr);
        delete ptr;
    }

    //  ref_intrusive(const ref_intrusive& rhs) copy consturctor base to base
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(first);
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(ifoo::destructor_called == false);
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive(const ref_intrusive<DerivedType>& rhs) copy consturctor derived to base
    {
        reset();
        iref<ibar> first(new ibar);
        first->_ifoo_member = "returned to global pool";
        iref<ifoo> second(first);
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(ifoo::destructor_called == false);
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    //  ref_intrusive(ref_intrusive&& rhs) move consturctor base to base
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(first.move());
        RASSERT(first.is_empty());
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive(ref_intrusive&& rhs) move consturctor derived to base
    {
        reset();
        iref<ibar> first(new ibar);
        first->_ifoo_member = "returned to global pool";
        iref<ifoo> second(first.move());
        RASSERT(first.is_empty());
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType> ref_intrusive& operator=(BaseOrDerivedType * rhs) assignment from pointer of base (simple policy)
    {
        reset();
        iref<ifoo> instance;
        instance = new ifoo;
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_intrusive& operator=(BaseOrDerivedType * rhs) assignment from pointer of derived (pooled policy - global)
    {
        reset();
        iref<ifoo> instance;
        instance = new ibar;
        instance->_ifoo_member = "return to global pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ibar::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("return to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    //  ref_intrusive& operator= (const ref_intrusive& rhs) assignment operator base to base
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second;
        second = first;
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(ifoo::destructor_called == false);
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive& operator= (const ref_intrusive& rhs) assignment operator base to base with same object_ptr
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(first);
        second = first;
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(ifoo::destructor_called == false);
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive& operator= (const ref_intrusive& rhs) assignment operator base self to self
    {
        reset();
        iref<ifoo> first(new ifoo);
        first = first;
        RASSERT(first.is_set());
        RASSERT(first.get_strong_refcount() == 1);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    //  ref_intrusive& operator= (const ref_intrusive<DerivedType>& rhs) assignment operator derived to base
    {
        reset();
        iref<ibar> first(new ibar);
        first->_ifoo_member = "returned to global pool";
        iref<ifoo> second;
        second = first;
        RASSERT(first.is_set());
        RASSERT(second.is_set());
        RASSERT(first.get_strong_refcount() == 2);
        RASSERT(second.get_strong_refcount() == 2);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(ifoo::destructor_called == false);
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        second.release();
        RASSERT(second.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // template<typename BaseOrDerivedType> bool operator==(const ref_intrusive<Type>& rhs)
    {
        reset();
        iref<ibar> first(new ibar);
        iref<ifoo> second(first);
        RASSERT(first == first);
        RASSERT(first == second);
        RASSERT(second == first);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) void create() with base class
    {
        reset();
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create();
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class and global pool
    {
        reset();
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create<ibar>();
        instance->_ifoo_member = "returned to global pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class and local pool
    {
        reset();
        ibar_pool pool;
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create<ibar>(&pool);
        instance->_ifoo_member = "returned to local pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ibar* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to local pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) pooled policy with base class and global pool
    {
        reset();
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ifoo>>();
        instance->_ifoo_member = "returned to global pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ifoo* ptr = ifoo_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) pooled policy with base class and local pool
    {
        reset();
        ifoo_pool pool;
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ifoo>>(&pool);
        instance->_ifoo_member = "returned to local pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ifoo* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to local pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    //template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) simple policy with derived class
    {
        reset();
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create<ibar, coid::ref_policy_simple<ibar>>();
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == true);
    }

    // template<typename BaseOrDerivedType, typename... PolicyArguments> void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments) with base class
    {
        reset();
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create(new ifoo);
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class and global pool
    {
        reset();
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create(new ibar());
        instance->_ifoo_member = "returned to global pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    // template<typename BaseOrDerivedType = Type, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) with derived class and local pool
    {
        reset();
        ibar_pool pool;
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create(new ibar(), &pool);
        instance->_ifoo_member = "returned to local pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ibar* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to local pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(Type* object_ptr, PolicyArguments&&... policy_arguments) pooled policy with base class and global pool
    {
        reset();
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ifoo>>(new ifoo);
        instance->_ifoo_member = "returned to global pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ifoo* ptr = ifoo_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    // template<typename Policy, typename... PolicyArguments> void create(PolicyArguments&&... policy_arguments) pooled policy with base class and local pool
    {
        reset();
        ifoo_pool pool;
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ifoo>>(new ifoo, &pool);
        instance->_ifoo_member = "returned to local pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == false);
        ifoo* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to local pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments) simple policy with derived class
    {
        reset();
        iref<ifoo> instance;
        RASSERT(instance.is_empty());
        instance.create<ibar, coid::ref_policy_simple<ibar>>(new ibar);
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called == true);
    }

    // template<typename DerivedType, typename Policy, typename... PolicyArguments> void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments)  explicit pooled policy with derived class and local pool
    {
        reset();
        ibar_pool pool;
        iref<ibar> instance;
        RASSERT(instance.is_empty());
        instance.create<coid::ref_policy_pooled<ibar>>(new ibar, &pool);
        instance->_ifoo_member = "returned to local pool";
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ibar::destructor_called == false);
        instance.release();
        RASSERT(instance.is_empty());
        RASSERT(ibar::destructor_called == false);
        ibar* ptr = pool.get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to local pool"));
        RASSERT(ptr->get_strong_refcount() == 0);
        delete ptr;
    }

    // ref_intrusive(nullptr_t) test
    {
        reset();
        iref<ifoo> instance(nullptr);
        RASSERT(instance.is_empty());
        RASSERT(instance.get() == nullptr);
    }

    // ~ref_intrusive() releases on scope exit without an explicit release()
    {
        reset();
        {
            iref<ifoo> first(new ifoo);
            {
                iref<ifoo> second(first);
                RASSERT(first.get_strong_refcount() == 2);
            }
            RASSERT(first.get_strong_refcount() == 1);
            RASSERT(ifoo::destructor_called == false);
        }
        RASSERT(ifoo::destructor_called);
    }

    // explicit operator bool() test
    {
        reset();
        iref<ifoo> instance;
        RASSERT(!static_cast<bool>(instance));
        instance.create();
        RASSERT(static_cast<bool>(instance));
    }

    // Type& operator*() and Type* get() test
    {
        reset();
        ifoo* ptr = new ifoo();
        iref<ifoo> instance(ptr);
        RASSERT(instance.get() == ptr);
        RASSERT(&(*instance) == ptr);
        RASSERT((*instance)._ifoo_member.cmpeq(ifoo::name));
    }

    // template<typename BaseOrDerivedType = Type> BaseOrDerivedType* get() const test
    {
        reset();
        iref<ifoo> instance;
        instance.create<ibar>();
        ibar* derived_ptr = instance.get<ibar>();
        RASSERT(derived_ptr != nullptr);
        RASSERT(static_cast<ifoo*>(derived_ptr) == instance.get());
        RASSERT(derived_ptr->_ifoo_member.cmpeq(ibar::name));
        instance->_ifoo_member = "returned to global pool";
        instance.release();
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // constructing from a pointer that is already reference counted adopts the existing policy
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(first.get());
        RASSERT(second.get() == first.get());
        RASSERT(first.get_strong_refcount() == 2);
        first.release();
        RASSERT(ifoo::destructor_called == false);
        second.release();
        RASSERT(ifoo::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_intrusive& operator=(BaseOrDerivedType* rhs)
    // assigning the pointer this ref already holds must not change the refcount
    {
        reset();
        iref<ifoo> instance(new ifoo);
        ifoo* ptr = instance.get();
        instance = ptr;
        RASSERT(instance.get() == ptr);
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
    }

    // template<typename BaseOrDerivedType> ref_intrusive& operator=(BaseOrDerivedType* rhs)
    // assigning a different pointer must release the previously held object
    {
        reset();
        iref<ifoo> instance(new ifoo);
        instance = new ifoo;
        RASSERT(ifoo::destructor_called);
        reset();
        RASSERT(instance.is_set());
        RASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        RASSERT(ifoo::destructor_called);
    }

    // template<typename BaseOrDerivedType> ref_intrusive& operator=(BaseOrDerivedType* rhs) with nullptr
    {
        reset();
        iref<ifoo> instance(new ifoo);
        instance = static_cast<ifoo*>(nullptr);
        RASSERT(instance.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    // const ref_intrusive& operator=(ref_intrusive<BaseOrDerivedType>&& rhs) move assignment base to base
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(new ifoo);
        second = first.move();
        RASSERT(first.is_empty());
        RASSERT(second.is_set());
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called);
        reset();
        second.release();
        RASSERT(second.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    // copy assignment from an empty ref has to clear the target
    {
        reset();
        iref<ifoo> empty_instance;
        iref<ifoo> assigned(new ifoo);
        assigned = empty_instance;
        RASSERT(assigned.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    // template<typename DerivedType> ref_intrusive<DerivedType> downcast() const
    {
        reset();
        iref<ifoo> instance;
        instance.create<ibar>();
        iref<ibar> downcasted = instance.downcast<ibar>();
        RASSERT(downcasted.is_set());
        RASSERT(static_cast<ifoo*>(downcasted.get()) == instance.get());
        RASSERT(downcasted->_ifoo_member.cmpeq(ibar::name));
        RASSERT(instance.get_strong_refcount() == 2);
        instance.release();
        RASSERT(downcasted.get_strong_refcount() == 1);
        downcasted->_ifoo_member = "returned to global pool";
        downcasted.release();
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // downcast() of an empty ref must stay empty
    {
        reset();
        iref<ifoo> instance;
        iref<ibar> downcasted = instance.downcast<ibar>();
        RASSERT(downcasted.is_empty());
    }

    // create<Policy>() has to recover the policy's original pointer as Policy::element_type*
    // before storing it - ifoo sits at +16 inside ibar, so both the object pointer and the
    // intrusive base need adjusting away from the policy's ibar*
    {
        reset();
        ifoo* stored_ptr = nullptr;
        {
            iref<ifoo> instance;
            instance.create<coid::ref_policy_pooled<ibar>>();
            RASSERT(instance.get_strong_refcount() == 1);
            stored_ptr = instance.get();
        }
        ibar* real_ptr = ibar_pool::global().get_item();
        RASSERT(real_ptr != nullptr);
        RASSERT(static_cast<void*>(real_ptr) != static_cast<void*>(static_cast<ifoo*>(real_ptr))); // sanity: offset is non-zero
        RASSERT(stored_ptr == static_cast<ifoo*>(real_ptr));
        RASSERT(real_ptr->get_strong_refcount() == 0);
        delete real_ptr;
    }

    // template<typename BaseOrDerivedType> ref_intrusive& operator=(ref_intrusive<BaseOrDerivedType>&&)
    // move assignment derived to base - the source has to end up empty and the refcount must not
    // gain an extra reference on the way
    {
        reset();
        iref<ibar> derived;
        derived.create();
        ibar* object_ptr = derived.get();
        iref<ifoo> base;
        base = derived.move();
        RASSERT(derived.is_empty());
        RASSERT(base.is_set());
        RASSERT(base.get() == static_cast<ifoo*>(object_ptr));
        RASSERT(base.get_strong_refcount() == 1);
        base->_ifoo_member = "returned to global pool";
        base.release();
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr == object_ptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // move assignment self to self must not touch the refcount - releasing here would drop the
    // only reference and destroy the object
    {
        reset();
        iref<ifoo> first(new ifoo);
        first = first.move();
        RASSERT(first.is_set());
        RASSERT(ifoo::destructor_called == false);
        RASSERT(first.get_strong_refcount() == 1);
        first.release();
        RASSERT(first.is_empty());
        RASSERT(ifoo::destructor_called);
    }

    // Two distinct refs holding the same object: moving one onto the other has to leave the
    // source empty and release exactly one reference, without destroying anything.
    {
        reset();
        iref<ifoo> first(new ifoo);
        iref<ifoo> second(first);
        RASSERT(first.get_strong_refcount() == 2);
        first = second.move();
        RASSERT(second.is_empty());
        RASSERT(first.is_set());
        RASSERT(ifoo::destructor_called == false);
        RASSERT(first.get_strong_refcount() == 1);
        first.release();
        RASSERT(ifoo::destructor_called);
    }

    // The same, across types - ibar's ifoo subobject is at +16, so the two refs hold different
    // addresses while sharing one refcount
    {
        reset();
        iref<ibar> derived;
        derived.create();
        iref<ifoo> base(derived);
        RASSERT(derived.get_strong_refcount() == 2);
        RASSERT(base.get() != static_cast<void*>(derived.get()));
        base = derived.move();
        RASSERT(derived.is_empty());
        RASSERT(base.is_set());
        RASSERT(base.get_strong_refcount() == 1);
        base.release();
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // a pooled object is recycled rather than destroyed, so it has to let go of its policy on the
    // way back to the pool - the policy is pooled too and will be handed to a different object
    {
        reset();
        ibar* first_ptr = ibar_pool::global().create_item();
        {
            iref<ibar> instance;
            instance.create(first_ptr);
        }

        ibar* recycled_ptr = ibar_pool::global().get_item();
        RASSERT(recycled_ptr == first_ptr);                 // same object came back
        RASSERT(ibar_pool::global().get_item() == nullptr);
        RASSERT(recycled_ptr->has_policy() == false);   // and with no policy on it

        iref<ibar> adopted(recycled_ptr);                   // must build a fresh policy for it
        iref<ibar> fresh;
        fresh.create();                                     // may well take the recycled policy

        RASSERT(adopted.get() != fresh.get());              // two distinct objects
        RASSERT(adopted.get_strong_refcount() == 1);        // ... with a refcount each
        RASSERT(fresh.get_strong_refcount() == 1);

        adopted.release();
        RASSERT(fresh.get_strong_refcount() == 1);          // releasing one must not touch the other
        fresh.release();

        ibar* a = ibar_pool::global().get_item();
        ibar* b = ibar_pool::global().get_item();
        RASSERT(a != nullptr && b != nullptr && a != b);    // both were pooled, neither leaked
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete a;
        delete b;
    }

    // a recycled intrusive object comes out of the pool with no policy attached, so the next ref
    // to adopt it starts a fresh count rather than inheriting whatever was there before
    {
        reset();
        {
            iref<ibar> instance;
            instance.create();
            instance->_ifoo_member = "first life";
        }
        ibar* recycled_ptr = ibar_pool::global().get_item();
        RASSERT(recycled_ptr != nullptr);
        RASSERT(recycled_ptr->_ifoo_member.cmpeq("first life"));  // object itself survived intact
        RASSERT(recycled_ptr->has_policy() == false);

        iref<ibar> second_life(recycled_ptr);
        RASSERT(second_life.get_strong_refcount() == 1);
        second_life.release();
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr == recycled_ptr);
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // the same policy is used for non intrusive types, where there is no back pointer to clear
    {
        reset();
        {
            ref<foo> instance;
            instance.create<bar>();
            instance->_foo_member = "returned to global pool";
        }
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }
    // get_strong_refcount() answers 0 for a ref that holds nothing, rather than dereferencing the
    // policy it does not have
    {
        reset();
        iref<ifoo> empty_instance;
        RASSERT(empty_instance.is_empty());
        RASSERT(empty_instance.get_strong_refcount() == 0);

        iref<ifoo> instance(new ifoo);
        RASSERT(instance.get_strong_refcount() == 1);
        instance.release();
        RASSERT(instance.get_strong_refcount() == 0);       // released is empty again

        iref<ifoo> source(new ifoo);
        iref<ifoo> target(source.move());
        RASSERT(source.get_strong_refcount() == 0);         // and so is moved from
        RASSERT(target.get_strong_refcount() == 1);
    }
    // ref_intrusive has no swap of its own - std::swap does the job through the move operations,
    // and must not disturb the refcount on the way
    {
        reset();
        ifoo* first_ptr = new ifoo();
        ifoo* second_ptr = new ifoo();
        iref<ifoo> first(first_ptr);
        iref<ifoo> second(second_ptr);
        std::swap(first, second);
        RASSERT(first.get() == second_ptr);
        RASSERT(second.get() == first_ptr);
        RASSERT(first.get_strong_refcount() == 1);
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
    }

    // the same with an empty ref on one side
    {
        reset();
        ifoo* ptr = new ifoo();
        iref<ifoo> first(ptr);
        iref<ifoo> second;
        std::swap(first, second);
        RASSERT(first.is_empty());
        RASSERT(second.get() == ptr);
        RASSERT(second.get_strong_refcount() == 1);
        RASSERT(ifoo::destructor_called == false);
    }

    // ref_intrusive_base::get_strong_refcount() and has_policy() - the object answers for itself,
    // no ref needed. These used to be free helpers in this file that read _policy_ptr out of the
    // object's memory, assuming it sits right after the vptr.

    // an intrusive object carries no policy until a ref adopts it - that is when one is created
    {
        reset();
        ifoo* object_ptr = new ifoo;
        RASSERT(object_ptr->has_policy() == false);
        RASSERT(object_ptr->get_strong_refcount() == 0);

        {
            iref<ifoo> instance(object_ptr);
            RASSERT(object_ptr->has_policy());
            RASSERT(object_ptr->get_strong_refcount() == 1);
        }

        // ifoo is unregistered, so its policy deleted it rather than recycling it
        RASSERT(ifoo::destructor_called);
    }

    // the count follows the refs, and can still be asked of the object after the last one is gone
    {
        reset();
        ifoo* object_ptr = ifoo_pool::global().create_item();
        {
            iref<ifoo> first;
            first.create<coid::ref_policy_pooled<ifoo>>(object_ptr, &ifoo_pool::global());
            RASSERT(object_ptr->has_policy());
            RASSERT(object_ptr->get_strong_refcount() == 1);

            iref<ifoo> second(first);
            RASSERT(object_ptr->get_strong_refcount() == 2);
            RASSERT(first.get_strong_refcount() == object_ptr->get_strong_refcount());

            second.release();
            RASSERT(object_ptr->get_strong_refcount() == 1);
        }

        // recycled into its pool, so the back pointer was cleared. has_policy() asks that
        // directly - a count of zero would also be reported by an object still pointing at a
        // policy that is merely sitting free in its pool, which is the 1.6 defect
        RASSERT(ifoo::destructor_called == false);
        ifoo* recycled_ptr = ifoo_pool::global().create_item();
        RASSERT(recycled_ptr == object_ptr);
        RASSERT(recycled_ptr->has_policy() == false);
        RASSERT(recycled_ptr->get_strong_refcount() == 0);
        delete recycled_ptr;
    }

    // Comparison against a raw pointer. Each ref declares one operator==(const Type*) and no
    // operator!=: C++20 synthesizes the negation and the reversed operand order from it, while
    // declaring != in the same scope would suppress both rewritings - MSVC C7692.

    // ref_intrusive against a raw pointer
    {
        reset();
        iref<ifoo> instance;
        instance.create();
        ifoo* true_ptr = instance.get();

        RASSERT(instance == true_ptr);
        RASSERT(true_ptr == instance);
        RASSERT((instance != true_ptr) == false);

        iref<ifoo> other;
        other.create();
        RASSERT(instance != other.get());
        RASSERT(other.get() != instance);

        other.release();
        instance.release();
        RASSERT(instance == nullptr);
        RASSERT(instance == static_cast<ifoo*>(nullptr));
    }
}

/// Multiple inheritance checks. "bar" derives from "boo" first and "foo" second, so foo sits at a
/// non-zero offset inside bar and every conversion between the two has to adjust the pointer.
void ref_multiple_inheritance_tests()
{
    // ref_shared holding a derived object through a base ref keeps the adjusted pointer
    {
        reset();
        bar* ptr = new bar();
        foo* adjusted_ptr = static_cast<foo*>(ptr);
        RASSERT(static_cast<void*>(adjusted_ptr) != static_cast<void*>(ptr)); // sanity: offset is non-zero
        ref<foo> instance(ptr);
        RASSERT(instance.get() == adjusted_ptr);
        RASSERT(instance->_foo_member.cmpeq(bar::name));
        instance->_foo_member = "returned to global pool";
        instance.release();
        bar* pooled_ptr = bar_pool::global().get_item();
        RASSERT(pooled_ptr == ptr);
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete pooled_ptr;
    }

    // copying derived -> base adjusts the pointer, both refs share one policy
    {
        reset();
        ref<bar> derived;
        derived.create();
        ref<foo> base(derived);
        RASSERT(base.get() == static_cast<foo*>(derived.get()));
        RASSERT(static_cast<void*>(base.get()) != static_cast<void*>(derived.get()));
        RASSERT(derived.get_strong_refcount() == 2);
        RASSERT(base->_foo_member.cmpeq(bar::name));
        derived->_foo_member = "returned to global pool";
        derived.release();
        base.release();
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // create<DerivedType>() through a base ref adjusts the pointer
    {
        reset();
        ref<foo> instance;
        instance.create<bar>();
        bar* derived_ptr = instance.downcast<bar>().get();
        RASSERT(instance.get() == static_cast<foo*>(derived_ptr));
        RASSERT(instance->_foo_member.cmpeq(bar::name));
        instance->_foo_member = "returned to global pool";
        instance.release();
        bar* ptr = bar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_foo_member.cmpeq("returned to global pool"));
        RASSERT(bar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // ref_intrusive: ibar derives from boo first and ifoo second, so the intrusive base is not at
    // offset zero either
    {
        reset();
        ibar* ptr = new ibar();
        ifoo* adjusted_ptr = static_cast<ifoo*>(ptr);
        RASSERT(static_cast<void*>(adjusted_ptr) != static_cast<void*>(ptr)); // sanity: offset is non-zero
        iref<ifoo> instance(ptr);
        RASSERT(instance.get() == adjusted_ptr);
        RASSERT(instance.get_strong_refcount() == 1);
        RASSERT(instance->_ifoo_member.cmpeq(ibar::name));
        instance->_ifoo_member = "returned to global pool";
        instance.release();
        ibar* pooled_ptr = ibar_pool::global().get_item();
        RASSERT(pooled_ptr == ptr);
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete pooled_ptr;
    }

    // ref_intrusive copy derived -> base and back through get<DerivedType>()
    {
        reset();
        iref<ibar> derived;
        derived.create();
        iref<ifoo> base(derived);
        RASSERT(base.get() == static_cast<ifoo*>(derived.get()));
        RASSERT(static_cast<void*>(base.get()) != static_cast<void*>(derived.get()));
        RASSERT(base.get<ibar>() == derived.get());
        RASSERT(derived.get_strong_refcount() == 2);
        derived->_ifoo_member = "returned to global pool";
        derived.release();
        base.release();
        ibar* ptr = ibar_pool::global().get_item();
        RASSERT(ptr != nullptr);
        RASSERT(ptr->_ifoo_member.cmpeq("returned to global pool"));
        RASSERT(ibar_pool::global().get_item() == nullptr);
        delete ptr;
    }

    // the accessors are members of the base, so the compiler adjusts for its offset - asking an
    // ibar* and the ifoo* inside it gives the same answer even though the addresses differ
    {
        reset();
        iref<ibar> instance;
        instance.create();
        ibar* true_ptr = instance.get();
        ifoo* base_ptr = static_cast<ifoo*>(true_ptr);
        RASSERT(static_cast<void*>(true_ptr) != static_cast<void*>(base_ptr));

        RASSERT(true_ptr->has_policy());
        RASSERT(true_ptr->get_strong_refcount() == 1);
        RASSERT(base_ptr->get_strong_refcount() == 1);

        iref<ifoo> as_base(instance);
        RASSERT(true_ptr->get_strong_refcount() == 2);
        RASSERT(base_ptr->get_strong_refcount() == 2);

        as_base.release();
        instance.release();
        RASSERT(ibar::destructor_called == false);
        ibar* recycled_ptr = ibar_pool::global().get_item();
        RASSERT(recycled_ptr == true_ptr);
        RASSERT(recycled_ptr->has_policy() == false);
        RASSERT(recycled_ptr->get_strong_refcount() == 0);
        delete recycled_ptr;
    }

    // an iref to a base and one to the derived type of the same object compare equal, although
    // their addresses differ - operator== adjusts, and ordering agrees with it
    {
        reset();
        iref<ibar> derived;
        derived.create();
        iref<ifoo> base(derived);
        RASSERT(static_cast<void*>(base.get()) != static_cast<void*>(derived.get()));
        RASSERT(base == derived);
        RASSERT(derived == base);                               // the reversed synthesized form

        iref<ifoo> base_copy(base);
        RASSERT((base < base_copy) == false);
        RASSERT((base_copy < base) == false);

        base_copy.release();
        base.release();
        derived.release();
        ibar* recycled_ptr = ibar_pool::global().get_item();
        RASSERT(recycled_ptr != nullptr);
        delete recycled_ptr;
    }

    // Comparison against a raw pointer. Each ref declares one operator==(const Type*) and no
    // operator!=: C++20 synthesizes the negation and the reversed operand order from it, while
    // declaring != in the same scope would suppress both rewritings - MSVC C7692.

    // multiple inheritance: the argument converts to Type* before the comparison, and that
    // conversion adjusts - so a base ref still matches the derived object's true address
    {
        reset();
        ref<bar> derived(new bar);
        bar* true_ptr = derived.get();
        ref<foo> base(derived);

        // the two refs hold different addresses
        RASSERT(static_cast<const void*>(base.get()) != static_cast<const void*>(true_ptr));
        // yet both compare equal to the derived pointer, because bar* -> foo* adjusts on the way in
        RASSERT(derived == true_ptr);
        RASSERT(base == true_ptr);
        RASSERT(true_ptr == base);
        // and the base ref matches its own adjusted pointer too
        RASSERT(base == base.get());

        base.release();
        derived.release();
        bar* recycled = bar_pool::global().get_item();
        RASSERT(recycled == true_ptr);
        delete recycled;
    }

    // the same for an intrusive type, where the pointer to the counting policy sits in a base
    // subobject rather than at offset zero
    {
        reset();
        iref<ibar> derived;
        derived.create();
        ibar* true_ptr = derived.get();
        iref<ifoo> base(derived);

        RASSERT(static_cast<const void*>(base.get()) != static_cast<const void*>(true_ptr));
        RASSERT(derived == true_ptr);
        RASSERT(base == true_ptr);
        RASSERT(true_ptr == base);

        base.release();
        derived.release();
        ibar* recycled = ibar_pool::global().get_item();
        RASSERT(recycled == true_ptr);
        delete recycled;
    }
}

/// Tests for defects that have just been fixed and are waiting to be reviewed. Once a fix is
/// signed off its block moves down into the regular suite of the type it belongs to and this
/// function shrinks back to empty.
void ref_tests_to_review()
{
}

void ref_tests()
{
    ref_unique_tests();
    ref_shared_tests();
    ref_intrusive_tests();
    ref_multiple_inheritance_tests();
    ref_tests_to_review();
}
