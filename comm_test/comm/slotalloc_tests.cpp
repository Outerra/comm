#include <comm/alloc/slotalloc.h>
#include <comm/commassert.h>

#include <utility>

/*
    Tests of the versioned ids of coid::slotalloc_versioning and coid::slotalloc_versioning_pool: a slot's version is bumped
    when the slot gets a new item (the first item has version 1), a deletion or a reset keeps it - a deleted item's id finds
    nothing (its slot's bit is clear) and never matches the slot's next item; the same for every way a slot gets an item
    (push, add, add_range, add_contiguous_range, add_uninit, get_or_create_uninit_internal, add_if) and for the slots freed by
    reset. In the pool mode a deleted item comes back by its id (undel_item) until its slot gets another item.

    Also: del_range across a page boundary destroys the items of its range; add_range and add_contiguous_range construct
    each item and its ext values once (also the slots they create; in the pool mode also the free slots that a contiguous
    range on a new page skips).

    And the lifetimes: the pool mode keeps every created slot's item and ext values constructed (deleted, reset, copied,
    created before a slot of get_or_create_uninit_internal) and destroys them all at discard and with the container; without the pool
    mode the ext values live with their slots' items (constructed by the allocation, destroyed once by the deletion, the
    reset or the discard); the uninitialized allocations construct neither the item nor its ext values, a reused pooled slot
    is returned as it is (is_new false - mandatory in the pool mode); add_uninit, add, add_new and add_if give the item's id
    (item_id_type - with versioning the versionid).
    An assignment destroys the items it replaces.

    And the item creation and deletion by id: create gives a new item in a free slot or past the created ones (nullptr
    for an occupied slot), constructed with its ext values; in the pool mode the slot's item - a new one, or a
    constructed one as it is; create_uninit leaves a new slot to the caller. del_item and undel_item take the item's id
    (without versioning the slot), push copies or moves the object in, add_new skips the free slots.

    Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

namespace {

/// @brief An item that counts its live instances and sums the values of the destroyed ones
struct versioned_item
{
    int _value = 0;

    static inline int alive = 0;
    static inline int destroyed_sum = 0;

    versioned_item() { ++alive; }
    explicit versioned_item(int value) : _value(value) { ++alive; }
    versioned_item(const versioned_item& other) : _value(other._value) { ++alive; }
    versioned_item& operator=(const versioned_item& other) { _value = other._value; return *this; }
    ~versioned_item() { --alive; destroyed_sum += _value; }
};

/// @brief A trivial item - for the pool mode (its deleted items stay constructed) and the uninitialized slots
struct plain_item
{
    int _value = 0;
};

/// @brief An item that counts its default constructions - for the pool mode (its free items stay constructed)
struct constructed_item
{
    int _value = 0;

    static inline int constructed = 0;

    constructed_item() { ++constructed; }
};

/// @brief An ext array value that counts its default constructions
struct ext_value
{
    static inline int constructed = 0;

    ext_value() { ++constructed; }
};

/// @brief An ext array value that counts its live instances
struct counted_ext
{
    static inline int alive = 0;

    counted_ext() { ++alive; }
    counted_ext(const counted_ext&) { ++alive; }
    counted_ext& operator=(const counted_ext&) = default;
    ~counted_ext() { --alive; }
};

/// @brief An item that records being moved from - for push(T&&)
struct movable_item
{
    int _value = 0;
    bool _moved_from = false;

    movable_item() = default;
    explicit movable_item(int value) : _value(value) {}
    movable_item(const movable_item& other) : _value(other._value) {}
    movable_item(movable_item&& other) noexcept : _value(other._value) { other._moved_from = true; }
    movable_item& operator=(const movable_item& other) { _value = other._value; return *this; }
    movable_item& operator=(movable_item&& other) noexcept { _value = other._value; other._moved_from = true; return *this; }
};

using versioned_items = coid::slotalloc_versioning<versioned_item>;

/// @brief The uninitialized allocations without is_new - not in the pool mode, where a reused pooled item is constructed
template<class A>
concept add_uninit_without_is_new = requires(A& a) { a.add_uninit(); };

static_assert(std::is_same_v<versioned_items::item_id_type, coid::versionid>);
static_assert(std::is_same_v<coid::slotalloc<plain_item>::item_id_type, uints>);

static_assert(add_uninit_without_is_new<coid::slotalloc<plain_item>>);
static_assert(!add_uninit_without_is_new<coid::slotalloc_pool<plain_item>>);
using plain_items = coid::slotalloc_versioning<plain_item>;
using pooled_items = coid::slotalloc_versioning_pool<plain_item>;

/// @brief A container with its get_or_create_uninit_internal public - only derived containers use it (slothash's
/// push_construct_in_slot)
template<class Base>
struct exposed : Base
{
    using Base::get_or_create_uninit_internal;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief A slot reused by push, add and add_range gets another version: the deleted item's id finds nothing
void reused_slot_gets_another_version()
{
    versioned_items items;

    const coid::versionid first_id = items.get_item_versionid(items.push(versioned_item(1)));
    RASSERT(first_id.ver32() == 1);
    items.del_item(first_id);
    RASSERT(!items.is_valid_id(first_id));

    // push into the freed slot
    const coid::versionid pushed_id = items.get_item_versionid(items.push(versioned_item(2)));
    RASSERT(pushed_id.id32() == first_id.id32());
    RASSERT(pushed_id != first_id);
    RASSERT(!items.is_valid_id(first_id) && items.is_valid_id(pushed_id));
    RASSERT(items.get_item(pushed_id)->_value == 2);

    // add into the freed slot
    items.del_item(pushed_id);
    const coid::versionid added_id = items.get_item_versionid(items.add());
    RASSERT(added_id.id32() == first_id.id32());
    RASSERT(added_id != first_id && added_id != pushed_id);
    RASSERT(!items.is_valid_id(first_id) && !items.is_valid_id(pushed_id) && items.is_valid_id(added_id));

    // add_range into freed slots
    coid::versionid range_ids[4];
    range_ids[0] = added_id;
    for (int i = 1; i < 4; ++i)
    {
        range_ids[i] = items.get_item_versionid(items.push(versioned_item(10 + i)));
    }

    for (const coid::versionid& id : range_ids)
    {
        items.del_item(id);
    }

    const uints first = items.add_range(4);
    RASSERT(items.count() == 4);
    for (const coid::versionid& id : range_ids)
    {
        RASSERT(!items.is_valid_id(id));
    }

    for (uints slot = first; slot < first + 4; ++slot)
    {
        const coid::versionid id = items.get_item_versionid(slot);
        RASSERT(items.is_valid_id(id));
        for (const coid::versionid& old_id : range_ids)
        {
            RASSERT(id != old_id);
        }
    }

    items.reset();
    RASSERT(versioned_item::alive == 0);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief add_range and add_contiguous_range over freed and new slots: the freed ones get other versions, the new ones
/// version 1
void add_range_versions()
{
    plain_items items;

    coid::versionid old_ids[2];
    for (int i = 0; i < 2; ++i)
    {
        old_ids[i] = items.get_item_versionid(items.push(plain_item{i}));
    }

    items.del_item(old_ids[0]);
    items.del_item(old_ids[1]);

    // slots 0 and 1 reused, 2 and 3 created
    const uints first = items.add_range(4);
    RASSERT(first == 0 && items.count() == 4);
    for (int i = 0; i < 2; ++i)
    {
        const coid::versionid id = items.get_item_versionid(uints(i));
        RASSERT(id != old_ids[i] && items.is_valid_id(id) && !items.is_valid_id(old_ids[i]));
    }

    for (uints slot = 2; slot < 4; ++slot)
    {
        RASSERT(items.get_item_versionid(slot).ver32() == 1);
    }

    // add_contiguous_range: slots 2 and 3 reused, 4 and 5 created
    const coid::versionid freed_ids[2] = { items.get_item_versionid(uints(2)), items.get_item_versionid(uints(3)) };
    items.del_item(freed_ids[0]);
    items.del_item(freed_ids[1]);

    items.add_contiguous_range(4);
    RASSERT(items.count() == 6);
    for (int i = 0; i < 2; ++i)
    {
        const coid::versionid id = items.get_item_versionid(uints(2 + i));
        RASSERT(id != freed_ids[i] && items.is_valid_id(id) && !items.is_valid_id(freed_ids[i]));
    }

    for (uints slot = 4; slot < 6; ++slot)
    {
        RASSERT(items.get_item_versionid(slot).ver32() == 1);
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief get_or_create_uninit_internal: a created slot (with the slots before it) gets version 1, a reused one another
/// version
void get_or_create_uninit_versions()
{
    exposed<plain_items> plain;

    const coid::versionid uninit_id = plain.get_item_versionid(new (plain.get_or_create_uninit_internal(1, nullptr)) plain_item{5});
    RASSERT(uninit_id.id32() == 1 && uninit_id.ver32() == 1);

    plain.del_item(uninit_id);
    const coid::versionid reused_id = plain.get_item_versionid(new (plain.get_or_create_uninit_internal(1, nullptr)) plain_item{6});
    RASSERT(reused_id.id32() == 1 && reused_id != uninit_id);
    RASSERT(!plain.is_valid_id(uninit_id) && plain.is_valid_id(reused_id) && plain.get_item(reused_id)->_value == 6);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief Reset ends the ids of its items: the slots created again get other versions, the old ids find nothing; slots past
/// the ones before the reset start from version 1
void reset_ends_the_ids()
{
    versioned_items items;

    coid::versionid old_ids[3];
    for (int i = 0; i < 3; ++i)
    {
        old_ids[i] = items.get_item_versionid(items.push(versioned_item(i)));
    }

    // a slot freed before the reset - its version goes on too
    items.del_item(old_ids[1]);

    items.reset();
    RASSERT(items.count() == 0 && versioned_item::alive == 0);
    for (const coid::versionid& id : old_ids)
    {
        RASSERT(!items.is_valid_id(id));
    }

    coid::versionid new_ids[4];
    for (int i = 0; i < 4; ++i)
    {
        new_ids[i] = items.get_item_versionid(items.push(versioned_item(100 + i)));
    }

    for (int i = 0; i < 3; ++i)
    {
        RASSERT(new_ids[i].id32() == old_ids[i].id32());
        RASSERT(new_ids[i] != old_ids[i]);
        RASSERT(!items.is_valid_id(old_ids[i]) && items.is_valid_id(new_ids[i]));
        RASSERT(items.get_item(new_ids[i])->_value == 100 + i);
    }

    // a new slot - its first item has version 1
    RASSERT(new_ids[3].id32() == 3 && new_ids[3].ver32() == 1 && items.is_valid_id(new_ids[3]));

    // reset twice: the versions go on
    items.reset();
    const coid::versionid again_id = items.get_item_versionid(items.push(versioned_item(7)));
    RASSERT(again_id.id32() == old_ids[0].id32() && again_id != old_ids[0] && again_id != new_ids[0]);
    RASSERT(!items.is_valid_id(new_ids[0]) && !items.is_valid_id(old_ids[0]));

    items.reset();
    RASSERT(versioned_item::alive == 0);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief A deleted pooled item comes back by its id (undel_item) until its slot gets another item, which ends the id
void pooled_item_comes_back_until_its_slot_is_reused()
{
    pooled_items items;

    const coid::versionid first_id = items.get_item_versionid(items.push(plain_item{1}));
    RASSERT(first_id.ver32() == 1);
    items.del_item(first_id);
    RASSERT(!items.is_valid_id(first_id));

    // the deleted item keeps its version
    const plain_item* undeleted = items.undel_item(first_id);
    RASSERT(undeleted != nullptr && undeleted->_value == 1);
    RASSERT(items.is_valid_id(first_id) && items.count() == 1);

    // the slot's next item gets another version
    items.del_item(first_id);
    const coid::versionid second_id = items.get_item_versionid(items.push(plain_item{2}));
    RASSERT(second_id.id32() == first_id.id32() && second_id != first_id);
    RASSERT(!items.is_valid_id(first_id) && items.is_valid_id(second_id));

    // deleted again - only the last item comes back
    items.del_item(second_id);
    RASSERT(items.undel_item(first_id) == nullptr);
    RASSERT(items.count() == 0);
    const plain_item* second = items.undel_item(second_id);
    RASSERT(second != nullptr && second->_value == 2 && items.is_valid_id(second_id));

    // add_if takes a deleted item the predicate accepts - another item in its slot, another version
    items.del_item(second_id);
    const plain_item* taken = items.add_if([](const plain_item& item) { return item._value == 2; });
    const coid::versionid taken_id = items.get_item_versionid(taken);
    RASSERT(taken_id.id32() == second_id.id32() && taken_id != second_id && items.count() == 1);
    RASSERT(!items.is_valid_id(second_id) && items.undel_item(second_id) == nullptr);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief The pool mode's reset keeps the versions: the slots' next items end the ids from before it, an expansion after it
/// keeps the versions of the slots in use
void pool_reset_keeps_the_versions()
{
    pooled_items items;

    coid::versionid old_ids[2];
    for (int i = 0; i < 2; ++i)
    {
        old_ids[i] = items.get_item_versionid(items.push(plain_item{i}));
    }

    items.reset();
    RASSERT(items.count() == 0);
    RASSERT(!items.is_valid_id(old_ids[0]) && !items.is_valid_id(old_ids[1]));

    // two reused slots, then a created one
    coid::versionid new_ids[3];
    for (int i = 0; i < 3; ++i)
    {
        new_ids[i] = items.get_item_versionid(items.push(plain_item{10 + i}));
    }

    for (int i = 0; i < 2; ++i)
    {
        RASSERT(new_ids[i].id32() == old_ids[i].id32() && new_ids[i] != old_ids[i]);
        RASSERT(!items.is_valid_id(old_ids[i]) && items.is_valid_id(new_ids[i]));
        RASSERT(items.get_item(new_ids[i])->_value == 10 + i);
    }

    RASSERT(new_ids[2].id32() == 2 && new_ids[2].ver32() == 1 && items.is_valid_id(new_ids[2]));
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief del_range across a page boundary (256 slots a page) destroys the items of its range
void del_range_across_pages()
{
    versioned_items items;
    for (int i = 0; i < 300; ++i)
    {
        items.push(versioned_item(i));
    }

    // slots 250 to 269
    int expected_sum = 0;
    for (int i = 250; i < 270; ++i)
    {
        expected_sum += i;
    }

    versioned_item::destroyed_sum = 0;
    items.del_range(250, 20);
    RASSERT(versioned_item::destroyed_sum == expected_sum);
    RASSERT(items.count() == 280 && versioned_item::alive == 280);
    for (uints slot = 0; slot < 300; ++slot)
    {
        RASSERT(items.is_valid_id(slot) == (slot < 250 || slot >= 270));
    }

    items.reset();
    RASSERT(versioned_item::alive == 0);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief add_range and add_contiguous_range construct each item and its ext values once
void ranges_construct_once()
{
    versioned_items items;

    // created slots
    items.add_range(3);
    RASSERT(items.count() == 3 && versioned_item::alive == 3);
    items.add_contiguous_range(3);
    RASSERT(items.count() == 6 && versioned_item::alive == 6);

    // slots 4 and 5 reused, 6 and 7 created
    items.del_range(4, 2);
    items.add_range(4);
    RASSERT(items.count() == 8 && versioned_item::alive == 8);

    // the first page has 248 free slots: a contiguous range of 250 on a new page, slots 256 to 505
    items.add_contiguous_range(250);
    RASSERT(items.count() == 258 && versioned_item::alive == 258);
    for (uints slot = 8; slot < 506; ++slot)
    {
        RASSERT(items.is_valid_id(slot) == (slot >= 256));
    }

    RASSERT(items.get_item_versionid(uints(256)).ver32() == 1 && items.get_item_versionid(uints(505)).ver32() == 1);

    items.reset();
    RASSERT(versioned_item::alive == 0);

    // the pool mode: the free slots skipped before the new page are constructed too
    coid::slotalloc_pool<constructed_item> pool;

    constructed_item::constructed = 0;
    pool.add_range(8);
    RASSERT(pool.count() == 8 && constructed_item::constructed == 8);

    pool.add_contiguous_range(250);
    RASSERT(pool.count() == 258 && constructed_item::constructed == 8 + 248 + 250);

    // the ext values
    coid::slotalloc<plain_item, ext_value> with_ext;

    ext_value::constructed = 0;
    with_ext.add_range(3);
    RASSERT(ext_value::constructed == 3);
    with_ext.add_contiguous_range(3);
    RASSERT(ext_value::constructed == 6);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief The pool mode destroys its free items too - at discard and with the container; its reset keeps the items and
/// their ext values
void pool_destroys_its_free_items()
{
    {
        coid::slotalloc_pool<versioned_item, counted_ext> pool;
        for (int i = 0; i < 5; ++i)
        {
            pool.push(versioned_item(i));
        }

        // deleted and reset items stay constructed, with their ext values
        pool.del_item(uints(1));
        pool.del_item(uints(3));
        RASSERT(pool.count() == 3 && versioned_item::alive == 5 && counted_ext::alive == 5);
        pool.reset();
        RASSERT(pool.count() == 0 && versioned_item::alive == 5 && counted_ext::alive == 5);

        // reused after the reset - the ext values kept, a created slot's constructed
        for (int i = 0; i < 6; ++i)
        {
            pool.push(versioned_item(10 + i));
        }

        RASSERT(pool.count() == 6 && versioned_item::alive == 6 && counted_ext::alive == 6);

        pool.discard();
        RASSERT(versioned_item::alive == 0 && counted_ext::alive == 0);

        // the container's destruction
        for (int i = 0; i < 3; ++i)
        {
            pool.push(versioned_item(i));
        }

        pool.del_item(uints(0));
        RASSERT(versioned_item::alive == 3 && counted_ext::alive == 3);
    }

    RASSERT(versioned_item::alive == 0 && counted_ext::alive == 0);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief A copy of a pool has its free items too: undel_item brings one back by its id, the copy destroys its own
void pool_copy_keeps_its_free_items()
{
    using pool_t = coid::slotalloc_versioning_pool<versioned_item, counted_ext>;

    {
        pool_t pool;

        coid::versionid ids[3];
        for (int i = 0; i < 3; ++i)
        {
            ids[i] = pool.get_item_versionid(pool.push(versioned_item(i)));
        }

        pool.del_item(ids[1]);

        {
            pool_t copy(pool);
            RASSERT(versioned_item::alive == 6 && counted_ext::alive == 6);
            RASSERT(copy.count() == 2 && copy.is_valid_id(ids[0]) && !copy.is_valid_id(ids[1]));

            const versioned_item* back = copy.undel_item(ids[1]);
            RASSERT(back != nullptr && back->_value == 1 && copy.is_valid_id(ids[1]));
            RASSERT(copy.get_item(ids[2])->_value == 2);
        }

        RASSERT(versioned_item::alive == 3 && counted_ext::alive == 3);
    }

    RASSERT(versioned_item::alive == 0 && counted_ext::alive == 0);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief get_or_create_uninit_internal in the pool mode constructs the free slots it creates before the slot; the slot is
/// left to the caller - its item and its ext values; add_uninit returns a reused pooled slot as it is
void pool_get_or_create_uninit_free_slots()
{
    {
        exposed<coid::slotalloc_pool<versioned_item, counted_ext>> pool;

        bool is_new = false;
        versioned_item* p = pool.get_or_create_uninit_internal(3, &is_new);
        RASSERT(is_new && pool.count() == 1 && versioned_item::alive == 3 && counted_ext::alive == 3);

        new (p) versioned_item(7);
        new (&pool.value_array<0>()[3]) counted_ext;
        RASSERT(versioned_item::alive == 4 && counted_ext::alive == 4);

        // add_uninit returns a reused pooled slot as it is - its item and ext values constructed (is_new false); slot 0 is
        // the lowest free one, created before slot 3
        pool.del_item(uints(3));
        uints id = UMAXS;
        is_new = true;
        p = pool.add_uninit(is_new, &id);
        RASSERT(!is_new && id == 0 && p->_value == 0);
        RASSERT(versioned_item::alive == 4 && counted_ext::alive == 4);
    }

    RASSERT(versioned_item::alive == 0 && counted_ext::alive == 0);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief Without the pool mode the ext values live with their slots' items: constructed by the allocation, destroyed once
/// by the deletion, the reset or the discard; the uninitialized allocations leave them to the caller
void ext_values_live_with_their_slots()
{
    {
        exposed<coid::slotalloc<plain_item, counted_ext>> items;
        for (int i = 0; i < 4; ++i)
        {
            items.push(plain_item{i});
        }

        RASSERT(counted_ext::alive == 4);

        items.del_item(uints(1));
        RASSERT(counted_ext::alive == 3);
        items.reset();
        RASSERT(counted_ext::alive == 0);

        // the uninitialized allocations construct neither - the caller constructs the ext values with the item
        uints id = UMAXS;
        plain_item* p = items.add_uninit(&id);
        RASSERT(id == 0 && counted_ext::alive == 0);
        new (p) plain_item{9};
        new (&items.value_array<0>()[id]) counted_ext;
        RASSERT(counted_ext::alive == 1);

        bool is_new = false;
        p = items.get_or_create_uninit_internal(7, &is_new);
        RASSERT(is_new && counted_ext::alive == 1);
        new (p) plain_item{10};
        new (&items.value_array<0>()[7]) counted_ext;
        RASSERT(counted_ext::alive == 2);

        items.discard();
        RASSERT(counted_ext::alive == 0);

        // the container's destruction
        items.push(plain_item{11});
        items.push(plain_item{12});
        items.del_item(uints(0));
        RASSERT(counted_ext::alive == 1);
    }

    RASSERT(counted_ext::alive == 0);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief An assignment destroys the items it replaces and copies the source's: paged, linear and in the pool mode; an
/// assignment to itself keeps the items
void assignment_replaces_the_items()
{
    {
        versioned_items source;
        coid::versionid ids[3];
        for (int i = 0; i < 3; ++i)
        {
            ids[i] = source.get_item_versionid(source.push(versioned_item(i)));
        }

        source.del_item(ids[1]);

        versioned_items target;
        for (int i = 0; i < 5; ++i)
        {
            target.push(versioned_item(10 + i));
        }

        RASSERT(versioned_item::alive == 7);

        target = source;
        RASSERT(versioned_item::alive == 4 && target.count() == 2 && target.allocated_count() == source.allocated_count());
        RASSERT(target.is_valid_id(ids[0]) && !target.is_valid_id(ids[1]) && target.get_item(ids[2])->_value == 2);

        const versioned_items& same = target;
        target = same;
        RASSERT(versioned_item::alive == 4 && target.count() == 2 && target.get_item(ids[0])->_value == 0);
    }

    RASSERT(versioned_item::alive == 0);

    {
        coid::slotalloc_linear<versioned_item> source;
        coid::slotalloc_linear<versioned_item> target;
        source.reserve_virtual(1024);
        target.reserve_virtual(1024);

        for (int i = 0; i < 3; ++i)
        {
            source.push(versioned_item(i));
        }

        for (int i = 0; i < 5; ++i)
        {
            target.push(versioned_item(10 + i));
        }

        target = source;
        RASSERT(versioned_item::alive == 6 && target.count() == 3 && target.allocated_count() == 3);
        RASSERT(target.get_item(uints(2))->_value == 2);
    }

    RASSERT(versioned_item::alive == 0);

    {
        using pool_t = coid::slotalloc_pool<versioned_item, counted_ext>;

        pool_t source;
        for (int i = 0; i < 3; ++i)
        {
            source.push(versioned_item(i));
        }

        source.del_item(uints(1));

        pool_t target;
        for (int i = 0; i < 5; ++i)
        {
            target.push(versioned_item(10 + i));
        }

        target = source;
        RASSERT(versioned_item::alive == 6 && counted_ext::alive == 6 && target.count() == 2);
    }

    RASSERT(versioned_item::alive == 0 && counted_ext::alive == 0);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief add_uninit gives the item's id: without versioning its slot, with versioning its versionid - also in the pool mode
/// (a reused pooled slot's next version)
void uninit_ids()
{
    {
        coid::slotalloc<plain_item> items;

        uints first = UMAXS;
        new (items.add_uninit(&first)) plain_item{1};
        uints second = UMAXS;
        new (items.add_uninit(&second)) plain_item{2};
        RASSERT(first == 0 && second == 1 && items.get_item(second)->_value == 2);
    }

    plain_items items;

    coid::versionid first_id;
    plain_item* p = items.add_uninit(&first_id);
    new (p) plain_item{3};
    RASSERT(first_id == items.get_item_versionid(p) && first_id.ver32() == 1 && items.is_valid_id(first_id));

    items.del_item(first_id);
    coid::versionid reused_id;
    new (items.add_uninit(&reused_id)) plain_item{4};
    RASSERT(reused_id.id32() == first_id.id32() && reused_id != first_id);
    RASSERT(items.is_valid_id(reused_id) && !items.is_valid_id(first_id));

    // the pool mode
    pooled_items pool;

    bool is_new = false;
    coid::versionid pooled_id;
    p = pool.add_uninit(is_new, &pooled_id);
    RASSERT(is_new);
    new (p) plain_item{5};
    RASSERT(pooled_id == pool.get_item_versionid(p) && pooled_id.ver32() == 1);

    pool.del_item(pooled_id);
    coid::versionid reused_pooled_id;
    p = pool.add_uninit(is_new, &reused_pooled_id);
    RASSERT(!is_new && p->_value == 5);
    RASSERT(reused_pooled_id.id32() == pooled_id.id32() && reused_pooled_id != pooled_id);
    RASSERT(pool.is_valid_id(reused_pooled_id) && !pool.is_valid_id(pooled_id));
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief add, add_new and add_if give the item's id: without versioning its slot, with versioning its versionid (a reused
/// slot's next version); add_new appends past a free slot; add_range of one item gives its slot
void add_ids()
{
    {
        coid::slotalloc<plain_item> items;

        uints slot = UMAXS;
        items.add(&slot);
        RASSERT(slot == 0);
        items.add_new(&slot);
        RASSERT(slot == 1);
        items.add(&slot);
        RASSERT(slot == 2);
        RASSERT(items.add_range(1) == 3);
    }

    plain_items items;

    coid::versionid added_id;
    const plain_item* p = items.add(&added_id);
    RASSERT(added_id == items.get_item_versionid(p) && added_id.ver32() == 1);

    items.del_item(added_id);
    coid::versionid reused_id;
    p = items.add(&reused_id);
    RASSERT(reused_id == items.get_item_versionid(p) && reused_id.id32() == added_id.id32() && reused_id != added_id);

    // a new slot past the free slot 0
    items.del_item(reused_id);
    coid::versionid new_id;
    p = items.add_new(&new_id);
    RASSERT(new_id == items.get_item_versionid(p) && new_id.id32() == 1 && new_id.ver32() == 1);
    RASSERT(!items.is_valid_id(uints(0)));

    // add_if: a reused pooled item the predicate accepts, else a new one past the free ones; a new one without free ones
    pooled_items pool;
    coid::versionid pushed_ids[2];
    for (int i = 0; i < 2; ++i)
    {
        pushed_ids[i] = pool.get_item_versionid(pool.push(plain_item{2 + i}));
    }

    pool.del_item(pushed_ids[0]);
    pool.del_item(pushed_ids[1]);

    coid::versionid taken_id;
    const plain_item* taken = pool.add_if([](const plain_item& item) { return item._value == 3; }, &taken_id);
    RASSERT(taken->_value == 3 && taken_id == pool.get_item_versionid(taken) && taken_id.id32() == 1 && taken_id.ver32() == 2);

    coid::versionid appended_id;
    const plain_item* appended = pool.add_if([](const plain_item&) { return false; }, &appended_id);
    RASSERT(appended_id == pool.get_item_versionid(appended) && appended_id.id32() == 2 && appended_id.ver32() == 1);

    pool.add();
    coid::versionid full_id;
    const plain_item* full = pool.add_if([](const plain_item&) { return true; }, &full_id);
    RASSERT(full_id == pool.get_item_versionid(full) && full_id.id32() == 3 && pool.count() == 4);
}

/// @brief create and create_uninit without the pool mode: a new item in a free slot or past the created ones, nullptr for
/// an occupied slot; create constructs the item and its ext values, create_uninit neither
void create_without_pool()
{
    versioned_item::alive = 0;
    counted_ext::alive = 0;
    {
        coid::slotalloc<versioned_item, counted_ext> items;

        // past the created slots - the slots before it stay free
        versioned_item* third = items.create(3);
        RASSERT(third != nullptr && items.get_item_id(third) == 3 && items.count() == 1);
        RASSERT(versioned_item::alive == 1 && counted_ext::alive == 1);

        // occupied
        RASSERT(items.create(3) == nullptr);
        RASSERT(items.count() == 1 && versioned_item::alive == 1);

        // a free slot
        versioned_item* first = items.create(1);
        RASSERT(first != nullptr && items.get_item_id(first) == 1 && items.count() == 2);
        RASSERT(versioned_item::alive == 2 && counted_ext::alive == 2);

        // the deletion destroys the item and its ext values
        items.del_item(1);
        RASSERT(items.count() == 1 && versioned_item::alive == 1 && counted_ext::alive == 1);
    }
    RASSERT(versioned_item::alive == 0 && counted_ext::alive == 0);

    // create_uninit: the caller constructs the item
    {
        coid::slotalloc<plain_item> items;
        plain_item* p = items.create_uninit(2);
        RASSERT(p != nullptr && items.get_item_id(p) == 2 && items.count() == 1);
        new (p) plain_item{7};
        RASSERT(items.create_uninit(2) == nullptr);
        RASSERT(items.get_item(2)->_value == 7);
    }
}

/// @brief create and create_uninit in the pool mode: the slot's item - newly constructed (is_new), or a constructed one
/// as it is: allocated already, or a free pooled one, which gets allocated
void create_in_pool()
{
    constructed_item::constructed = 0;
    ext_value::constructed = 0;

    coid::slotalloc_pool<constructed_item, ext_value> items;
    bool is_new = false;

    // past the created slots: the free slots 0 and 1 are constructed too, with their ext values
    constructed_item* second = items.create(2, is_new);
    RASSERT(second != nullptr && is_new && items.get_item_id(second) == 2 && items.count() == 1);
    RASSERT(constructed_item::constructed == 3 && ext_value::constructed == 3);
    second->_value = 5;

    // allocated already: the item as it is, nothing allocated or constructed
    constructed_item* again = items.create(2, is_new);
    RASSERT(again == second && !is_new && again->_value == 5 && items.count() == 1);
    RASSERT(constructed_item::constructed == 3);

    // a free pooled slot: its constructed item, allocated now
    constructed_item* first = items.create(0, is_new);
    RASSERT(first != nullptr && !is_new && items.get_item_id(first) == 0 && items.count() == 2);
    RASSERT(constructed_item::constructed == 3 && ext_value::constructed == 3);

    // create_uninit past the created slots: slot 3 in between is constructed, slot 4 is left to the caller
    constructed_item* fifth = items.create_uninit(4, is_new);
    RASSERT(fifth != nullptr && is_new && items.get_item_id(fifth) == 4 && items.count() == 3);
    RASSERT(constructed_item::constructed == 4 && ext_value::constructed == 4);
    new (fifth) constructed_item;
}

/// @brief del_item and undel_item by the item's id without versioning, where the id is the slot
void del_and_undel_by_slot_id()
{
    coid::slotalloc_pool<plain_item> items;
    items.push(plain_item{1});
    items.push(plain_item{2});

    items.del_item(0);
    RASSERT(items.count() == 1 && !items.is_valid_id(0));

    plain_item* back = items.undel_item(0);
    RASSERT(back != nullptr && back->_value == 1 && items.count() == 2 && items.is_valid_id(0));
}

/// @brief push copies or moves the object in, also into a reused pooled slot
void push_copies_and_moves()
{
    {
        coid::slotalloc<movable_item> items;
        movable_item original(3);
        movable_item* copied = items.push(original);
        RASSERT(copied->_value == 3 && !original._moved_from);

        movable_item source(4);
        movable_item* moved = items.push(std::move(source));
        RASSERT(moved->_value == 4 && source._moved_from);
    }

    // the pool mode: the free pooled item gets the pushed value
    {
        coid::slotalloc_pool<movable_item> items;
        items.push(movable_item(1));
        items.del_item(0);

        movable_item source(8);
        movable_item* reused = items.push(std::move(source));
        RASSERT(items.get_item_id(reused) == 0 && reused->_value == 8 && source._moved_from);
    }
}

/// @brief add_new appends past the created slots and leaves the free ones free
void add_new_skips_free_slots()
{
    coid::slotalloc<plain_item> items;
    items.push(plain_item{1});
    items.push(plain_item{2});
    items.del_item(0);

    uints id = UMAXS;
    plain_item* appended = items.add_new(&id);
    RASSERT(id == 2 && items.get_item_id(appended) == 2 && items.count() == 2 && !items.is_valid_id(0));
}

} // end of anonymous namespace

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void slotalloc_tests()
{
    reused_slot_gets_another_version();
    add_range_versions();
    get_or_create_uninit_versions();
    reset_ends_the_ids();
    pooled_item_comes_back_until_its_slot_is_reused();
    pool_reset_keeps_the_versions();
    del_range_across_pages();
    ranges_construct_once();
    pool_destroys_its_free_items();
    pool_copy_keeps_its_free_items();
    pool_get_or_create_uninit_free_slots();
    ext_values_live_with_their_slots();
    assignment_replaces_the_items();
    uninit_ids();
    add_ids();
    create_without_pool();
    create_in_pool();
    del_and_undel_by_slot_id();
    push_copies_and_moves();
    add_new_skips_free_slots();
}
