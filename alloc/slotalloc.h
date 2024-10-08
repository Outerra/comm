#pragma once

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * Outerra.
 * Portions created by the Initial Developer are Copyright (C) 2013-2020
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <new>
#include <atomic>
#include "../namespace.h"
#include "../commexception.h"
#include "../dynarray.h"
#include "../bitrange.h"
#include "../trait.h"

#include "slotalloc_tracker.h"

COID_NAMESPACE_BEGIN

#if defined(COID_CONSTEXPR_IF) && !defined(__cpp_if_constexpr)
#error Please enable C++17 language standard (/std:c++17) in project settings for VS2017+ projects
#endif

#ifndef COID_CONCEPTS
#error Please enable C++20 language standard (/std:c++20) in project settings for VS2017+ projects
#endif

////////////////////////////////////////////////////////////////////////////////
/**
@brief Allocator for efficient slot allocation/deletion of objects.

Allocates array of slots for given object type, and allows efficient allocation and deallocation of objects without
having to resort to expensive system memory allocators.
Objects within the array have a unique slot id that can be used to retrieve the objects by id or to have an id of the
object during its lifetime.
The allocator has for_each and find_if methods that can run functors on each object managed by the allocator.

Optionally can be constructed in pool mode, in which case the removed/freed objects aren't destroyed, and subsequent
allocation can return one of these without having to call the constructor. This is good when the objects allocate
their own memory buffers and it's desirable to avoid unnecessary freeing and allocations there as well. Obviously,
care must be taken to initialize the initial state of allocated objects in this mode.
All objects are destroyed with the destruction of the allocator object.

Safe to use from a single producer / single consumer threading mode, as long as the working set is
reserved in advance.

@param T array element type
@param MODE see slotalloc_mode flags
@param Es variadic types for optional parallel arrays which will be managed along with the main array
**/
template<class T, slotalloc_mode MODE = slotalloc_mode::base, class ...Es>
class slotalloc_base
    : protected slotalloc_detail::storage<MODE & slotalloc_mode::linear, MODE & slotalloc_mode::atomic, T>
    , protected slotalloc_detail::base<MODE & slotalloc_mode::versioning, MODE & slotalloc_mode::tracking, Es...>
{
protected:

    using base_t = slotalloc_base<T, MODE, Es...>;
    using tracker_t = slotalloc_detail::base<MODE & slotalloc_mode::versioning, MODE & slotalloc_mode::tracking, Es...>;
    using storage_t = slotalloc_detail::storage<MODE & slotalloc_mode::linear, MODE & slotalloc_mode::atomic, T>;

    using extarray_t = typename tracker_t::extarray_t;
    using changeset_t = slotalloc_detail::changeset;

    using bitmask_type = typename storage_t::bitmask_type;

    static constexpr int BITMASK_BITS = 8 * sizeof(uints);

    static constexpr bool POOL = (MODE & slotalloc_mode::pool) != 0;
    static constexpr bool ATOMIC = (MODE & slotalloc_mode::atomic) != 0;
    static constexpr bool TRACKING = (MODE & slotalloc_mode::tracking) != 0;
    static constexpr bool VERSIONING = (MODE & slotalloc_mode::versioning) != 0;
    static constexpr bool LINEAR = (MODE & slotalloc_mode::linear) != 0;

public:

    ///Construct slotalloc container
    /// @note it's required to reserve memory or virtual address space, and/or to call allow_rebase() to indicate that it's ok when item addresses change on resize
    slotalloc_base()
    {}

    ///Constructor, reserve memory (non-virtual, will rebase when overflows)
    /// @param reserve_items number of items to reserve memory for
    /// @param bvirtual true for virtual address space reservation, false for physical reservation (will rebase if crossed)
    slotalloc_base(uints nitems, reserve_mode mode)
    {
        if (mode == reserve_mode::virtual_space)
            reserve_virtual(nitems);
        else
            reserve(nitems);
    }

    ~slotalloc_base() {
        if coid_constexpr_if(!POOL)
            reset();

        discard();
    }

    slotalloc_base(const slotalloc_base& s) {
        copy(s);
    }

    void copy(const slotalloc_base& o)
    {
        _allocated = o._allocated;

        if coid_constexpr_if(LINEAR) {
            copy_array_with_bitmask(o._array, this->_array, _allocated);
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            this->prepare_storage_copy(o);

            bitmask_type const* bm = const_cast<bitmask_type const*>(_allocated.ptr());
            bitmask_type const* em = const_cast<bitmask_type const*>(_allocated.ptre());
            bitmask_type const* pm = bm;
            uints gbase = 0;

            for (uints ip = 0; ip < this->_pages.size(); ++ip, gbase += page::ITEMS)
            {
                T* dd = const_cast<T*>(this->_pages[ip].ptr());
                const T* ds = o._pages[ip].ptr();

                bitmask_type const* epm = em - pm > page::NMASK
                    ? pm + page::NMASK
                    : em;

                uints pbase = 0;

                for (; pm != epm; ++pm, pbase += BITMASK_BITS) {
                    if (*pm == 0)
                        continue;

                    uints m = 1;
                    for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                        if (*pm & m)
                            new (dd + (pbase + i)) T(ds[pbase + i]);
                        else if ((*pm & ~(m - 1)) == 0)
                            break;
                    }
                }
            }
        }

        extarray_copy(o);
    }

    /// @return value from ext array associated with given main array object
    template<size_t V>
    typename std::tuple_element<V, extarray_t>::type::value_type&
        assoc_value(const T* p) {
        return std::get<V>(this->_exts)[get_item_id(p)];
    }

    /// @return value from ext array associated with given main array object
    template<size_t V>
    const typename std::tuple_element<V, extarray_t>::type::value_type&
        assoc_value(const T* p) const {
        return std::get<V>(this->_exts)[get_item_id(p)];
    }

    /// @return value from ext array for given index
    template<size_t V>
    typename std::tuple_element<V, extarray_t>::type::value_type&
        value(uints index) {
        return std::get<V>(this->_exts)[index];
    }

    /// @return value from ext array for given index
    template<size_t V>
    const typename std::tuple_element<V, extarray_t>::type::value_type&
        value(uints index) const {
        return std::get<V>(this->_exts)[index];
    }

#ifdef __cpp_lib_tuples_by_type

    /// @return value from ext array for given type
    template<typename T2>
    T2& value_type(uints index) {
        return std::get<T2>(this->_exts)[index];
    }

    /// @return value from ext array for given type
    template <typename T2>
    const T2& value_type(uints index) const {
        return std::get<T2>(this->_exts)[index];
    }

#endif

    /// @return ext array
    template<size_t V>
    typename std::tuple_element<V, extarray_t>::type&
        value_array() {
        return std::get<V>(this->_exts);
    }

    /// @return ext array
    template<size_t V>
    const typename std::tuple_element<V, extarray_t>::type&
        value_array() const {
        return std::get<V>(this->_exts);
    }

    void swap(slotalloc_base& other) {
        this->swap_storage(other);
        this->swap_exts(other);

        std::swap(_allocated, other._allocated);
        std::swap(_count, other._count);
    }

    friend void swap(slotalloc_base& a, slotalloc_base& b) {
        a.swap(b);
    }

    void reserve(uints nitems)
    {
        uints na = align_to_chunks(nitems, BITMASK_BITS);

        if coid_constexpr_if (LINEAR) {
            this->_array.reserve(nitems, true);
        }
        else {
            this->_pages.reserve(na, true);
        }

        _allocated.reserve(na, true);

        extarray_reserve(nitems, reserve_mode::memory);
    }

    void reserve_virtual(uints nitems)
    {
        uints na = align_to_chunks(nitems, BITMASK_BITS);

        if coid_constexpr_if (LINEAR) {
            discard();

            this->_array.reserve_virtual(nitems);
        }
        else {
            this->_pages.reserve_virtual(na);
        }

        _allocated.reserve_virtual(na);

        extarray_reserve(nitems, reserve_mode::virtual_space);
    }


    ///Insert object
    /// @return pointer to the newly inserted object
    T* push(const T& v)
    {
        uints id = -1;
        bool isold = _count < created();
        T* p = isold ? alloc(&id) : append<false>(&id);

        extarray_construct_default(id, isold);

        return copy_object(p, isold, v);
    }

    ///Insert object
    /// @return pointer to the newly inserted object
    T* push(T&& v)
    {
        uints id = -1;
        bool isold = _count < created();
        T* p = isold ? alloc(&id) : append<false>(&id);

        extarray_construct_default(id, isold);

        return this->copy_object(p, isold, std::forward<T>(v));
    }

    ///Add new object initialized with constructor matching the arguments
    template<class...Ps>
    T* push_construct(Ps&&... ps)
    {
        uints id = -1;
        bool isold = _count < created();
        T* p = isold ? alloc(&id) : append<false>(&id);

        extarray_construct_default(id, isold);

        return construct_object(p, isold, std::forward<Ps>(ps)...);
    }

    ///Add new object initialized with default constructor, or reuse one in pool mode
    T* add(uints* pid = 0)
    {
        uints id = -1;
        bool isold = _count < created();
        T* p = isold ? alloc(&id) : append<false>(&id);

        if (pid)
            *pid = id;

        extarray_construct_default(id, isold);

        return construct_default(p, isold);
    }

    ///Add completely new object, ignoring any unused ones
    T* add_new(uints* pid = 0)
    {
        uints id = -1;
        T* p = append<false>(&id);

        if (pid)
            *pid = id;


        extarray_construct_default(get_item_id(p), false);

        return construct_default(p, false);
    }

    ///Add new object or reuse one from pool if predicate returns true
    template <typename Func>
    T* add_if(Func fn, uints* pid = 0) COID_REQUIRES((POOL))
    {
        bool isold = _count < created();

        if (!isold)
        {
            uints id = -1;
            T* result = new(append<false>(&id)) T;
            extarray_construct_default(id, false);
            if (pid)
                *pid = id;

            return result;
        }

        uints unused_id = find_unused(fn);

        if (unused_id != UMAXS)
        {
            return alloc_item(pid ? (*pid = unused_id) : unused_id);
        }
        else
        {
            uints id = -1;
            T* result = new(append<false>(&id)) T;
            extarray_construct_default(id, false);
            if (pid)
                *pid = id;

            return result;
        }
    }

    ///Add new object, uninitialized (no constructor invoked on the object)
    /// @param newitem optional variable that receives whether the object slot was newly created (true) or reused from the pool (false)
    /// @note if newitem == 0 within the pool mode and thus no way to indicate the item has been reused, the reused objects have destructors called
    T* add_uninit(bool* newitem = 0, uints* pid = 0)
    {
        if (_count < created()) {
            T* p = alloc(pid);
            if coid_constexpr_if(POOL) {
                if (!newitem) destroy(*p);
                else *newitem = false;
            }
            else if (newitem)
                *newitem = true;
            return p;
        }
        if (newitem) *newitem = true;
        return append<true>(pid);
    }

    ///Add range of objects initialized with default constructors
    /// @return id to the beginning of the allocated range
    /// @note The range is can be non contiguous in case of paged slotalloc (the range can cross the page boundary)
    uints add_range(uints n)
    {
        if (n == 0)
            return UMAXS;
        if (n == 1) {
            uints id;
            add(&id);
            return id;
        }

        uints nold;
        uints id = alloc_range<false>(n, &nold);

        for_range_unchecked(id, n, [&](T* p, uints id) {
            construct_default(p, nold > 0);
            extarray_construct_default(id, nold > 0);
            if (nold)
                nold--;
            });

        return id;
    }

    ///Add contiguous range of objects initialized with default constructors
    /// @return pointer to the first item or nullptr if contiguous can't be allocated
    T* add_contiguous_range(uints n)
    {
        if coid_constexpr_if (!LINEAR) {
            if (n > storage_t::page::ITEMS)
                return 0;
        }

        if (n == 0)
            return 0;
        if (n == 1)
            return add();

        uints nold;
        uints id = alloc_range_contiguous<false>(n, &nold);

        for_range_unchecked(id, n, [&](T* p, uints id) {
            construct_default(p, nold > 0);
            extarray_construct_default(id, nold > 0);
            if (nold)
                nold--;
            });

        return ptr(id);
    }

    ///Delete object by pointer
    void del_item_by_ptr(T* p)
    {
        del_item_internal(p, get_item_id(p));
    }

    ///Delete object by id
    void del_item(uints id)
    {
        DASSERT_RET(id < created());
        del_item_internal(ptr(id), id);
    }


    ///Delete object by versionid
    void del_item(versionid vid) COID_REQUIRES((VERSIONING))
    {
        DASSERT_RET(this->check_versionid(vid));

        return del_item(vid.idx);
    }

    /// @return previously deleted but still valid item
    /// @note only works in POOL mode
    T* undel_item(uints id) COID_REQUIRES((POOL))
    {
        static_assert(POOL, "only available in pool mode");

        if (POOL && id < created()) {
            if (!set_bit(id))
                ++_count;
            else {
                DASSERTN(0);
                return 0;
            }

            return ptr(id);
        }

        return 0;
    }

    /// @return previously deleted but still valid item with correct version
    /// @note only works in POOL mode
    T* undel_item(versionid vid) COID_REQUIRES((VERSIONING && POOL))
    {
        static_assert(POOL, "only available in pool mode");
        uint id = uint(vid);

        if (POOL && id < this->_created) {
            if (this->check_versionid(vid)) {
                if (!set_bit(id))
                    ++_count;
                else {
                    DASSERTN(0);
                    return 0;
                }
            }

            return ptr(id);
        }

        return 0;
    }

    ///Mark object deleted without running the destructor
    /// @note useful for deletion of objects from within item destructors
    void del_nodestruct(T* p)
    {
        static_assert(!POOL, "error: cannot be used in pool mode");

        uints id = get_item_id(p);
        if (id >= created())
            throw exception("attempting to delete an invalid object ") << id;

        DASSERT_RET(get_bit(id));

        this->set_modified(id);
        this->bump_version(id);

        if (clear_bit(id))
            --_count;
        else
            DASSERTN(0);
    }

    void del_range(uints item_id, uints n) {
        if (n == 0)
            return;
        if (n == 1)
            return del_item(item_id);

        uints idk = item_id;

        if coid_constexpr_if(LINEAR) {
            auto b = this->_array.ptr() + item_id;
            auto e = b + n;

            for (; b < e; ++b) {
                if coid_constexpr_if(!POOL)
                {
                    b->~T();
                    extarray_destruct(idk);
                }
                this->bump_version(idk++);
            }
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            uint pg = uint(item_id / page::ITEMS);
            uint s = uint(item_id % page::ITEMS);
            uints nr = n;

            while (nr > 0) {
                T* b = this->_pages[pg].ptr() + s;
                uints na = stdmin(page::ITEMS - s, nr);
                T* e = b + na;

                for (; b < e; ++b) {
                    if coid_constexpr_if(!POOL)
                    {
                        b->~T();
                        extarray_destruct(idk);
                    }
                    this->bump_version(idk++);
                }

                nr -= na;
                s = 0;
            }
        }
        _count -= clear_bitrange(item_id, n, _allocated.ptr());
    }

    ///Del range of objects
    void del_range(T* p, uints n)
    {
        del_range(get_item_id(p), n);
    }

    /// @return number of used slots in the container
    uints count() const { return _count; }

    /// @return allocated and previously created count (not necessarily used currently)
    uints allocated_count() const {
        return created();
    }

    /// @return number of currently preallocated items
    uints preallocated_count() const {
        if coid_constexpr_if (LINEAR)
            return this->_array.reserved_total() / sizeof(T);
        else
            return this->_pages.size() * storage_t::page::ITEMS;
    }

    /// @{ accessors with versionid argument, enabled only if versioning is on

    ///Return an item given id
    /// @param id id of the item
    const T* get_item(versionid vid) const COID_REQUIRES((VERSIONING))
    {
        DASSERT_RET(this->check_versionid(vid) && get_bit(vid.idx), 0);
        return ptr(vid.idx);
    }

    ///Return an item given id
    /// @param id id of the item
    /// @note non-const operator [] disabled on tracking allocators, use explicit get_mutable_item to indicate the element will be modified
    T* get_item(versionid vid) COID_REQUIRES((VERSIONING))
    {
        DASSERT_RET(this->check_versionid(vid) && get_bit(vid.idx), 0);
        return ptr(vid.idx);
    }

    ///Return an item given id
    /// @param id id of the item
    T* get_mutable_item(versionid vid) COID_REQUIRES((VERSIONING))
    {
        DASSERT_RET(this->check_versionid(vid) && get_bit(vid.idx), 0);
        this->set_modified(vid.idx);

        return ptr(vid.idx);
    }

    const T& operator [] (versionid vid) const COID_REQUIRES((VERSIONING))
    {
        return *get_item(vid);
    }

    /// @note non-const operator [] disabled on tracking allocators, use explicit get_mutable_item to indicate the element will be modified
    T& operator [] (versionid vid) COID_REQUIRES((VERSIONING && !TRACKING))
    {
        return *get_mutable_item(vid);
    }

    /// @}


    ///Return an item given id
    /// @param id id of the item
    const T* get_item(uints id) const
    {
        DASSERT_RET(id < created() && get_bit(id), 0);
        return ptr(id);
    }

    ///Return an item given id
    /// @param id id of the item
    /// @note non-const operator [] disabled on tracking allocators, use explicit get_mutable_item to indicate the element will be modified
    T* get_item(uints id) COID_REQUIRES((!TRACKING))
    {
        DASSERT_RET(id < created() && get_bit(id), 0);
        return ptr(id);
    }

    ///Return an item given id
    /// @param id id of the item
    T* get_mutable_item(uints id)
    {
        DASSERT_RET(id < created() && get_bit(id), 0);
        this->set_modified(id);

        return ptr(id);
    }

    const T& operator [] (uints id) const {
        return *get_item(id);
    }

    /// @note non-const operator [] disabled on tracking allocators, use explicit get_mutable_item to indicate the element will be modified
    T& operator [] (uints id) COID_REQUIRES((!TRACKING))
    {
        return *get_mutable_item(id);
    }


    ///Get a particular item from given slot or default-construct a new one there
    /// @param id item id, reverts to add() if UMAXS
    /// @param is_new optional if not null, receives true if the item was newly created
    /// @note a deleted item in POOL mode is also considered newly created here
    T* get_or_create(uints id, bool* is_new = 0)
    {
        if (id == UMAXS) {
            if (is_new) *is_new = true;
            return add();
        }

        if (id < created()) {
            //within allocated space
            this->set_modified(id);

            T* p = ptr(id);

            if (get_bit(id)) {
                //existing object
                if (is_new) *is_new = false;
                return p;
            }

            if coid_constexpr_if(!POOL)
                new(p) T;

            set_bit(id);

            ++_count;

            if (is_new) *is_new = true;
            return p;
        }

        //extra space needed
        uints n = id + 1 - created();

        expand<false, false>(n);

        this->set_modified(id);

        set_bit(id);

        ++_count;

        if (is_new) *is_new = true;
        return ptr(id);
    }

    ///Get a particular item from given slot or default-construct a new one there
    /// @param id item id, reverts to add() if UMAXS
    /// @param is_new optional if not null, receives true if the item was newly created (also not restored from pool)
    T* get_or_create_uninit(uints id, bool* is_new = 0)
    {
        if (id == UMAXS) {
            if (is_new) *is_new = true;
            return add_uninit();
        }

        if (id < created()) {
            //within allocated space
            this->set_modified(id);

            T* p = ptr(id);

            if (get_bit(id)) {
                //existing object
                if (is_new) *is_new = false;
                return p;
            }

            set_bit(id);

            ++_count;

            if (is_new) *is_new = !POOL;
            return p;
        }

        //extra space needed
        uints n = id + 1 - created();

        expand<true, true>(n);

        this->set_modified(id);

        set_bit(id);

        ++_count;

        if (is_new) *is_new = true;
        return ptr(id);
    }

    /// @return id of given item, or UMAXS if the item is not managed here
    uints get_item_id(const T* p) const
    {
        if coid_constexpr_if (LINEAR) {
            uints id = p - this->_array.ptr();
            return id < this->_array.size()
                ? id
                : UMAXS;
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            uints id = 0;

            for (const page& pg : this->_pages) {
                if (p >= pg.ptr() && p < pg.ptre())
                    return id + (p - pg.ptr());

                id += page::ITEMS;
            }

            return UMAXS;
        }
    }

    /// @return if of given item in ext array or UMAXS if the item is not managed here
    template<int V, class K>
    uints get_array_item_id(const K* p) const COID_REQUIRES((VERSIONING))
    {
        auto& array = value_array<V>();
        uints id = p - array.ptr();
        return id < array.size()
            ? id
            : UMAXS;
    }

    /// @return versionid of given item
    versionid get_item_versionid(const T* p) const COID_REQUIRES((VERSIONING))
    {
        uints id = get_item_id(p);
        return id == UMAXS
            ? versionid()
            : this->get_versionid(id);
    }

    /// @return versionid of given slot_id if valid
    versionid get_item_versionid(uints slot_id) const COID_REQUIRES((VERSIONING))
    {
        return is_valid_id(slot_id)
            ? this->get_versionid(slot_id)
            : versionid();
    }

    /// @return true if item with id is valid
    bool is_valid_id(uints id) const {
        return get_bit(id);
    }

    /// @return true if item with id is valid
    bool is_valid_id(versionid vid) const COID_REQUIRES((VERSIONING))
    {
        return this->check_versionid(vid) && get_bit(vid.idx);
    }

    /// @return true if item is valid
    bool is_valid(const T* p) const {
        return get_bit(get_item_id(p));
    }

    bool operator == (const slotalloc_base& other) const {
        if (_count != other._count)
            return false;

        const T* dif = find_if([&](const T& v, uints id) {
            return !(v == other[id]);
            });

        return dif == 0;
    }


    const dynarray<T>* linear_array() const {
        return storage_t::linear_array();
    }


    ///Reset content. Destructors aren't invoked in the pool mode, as the objects may still be reused.
    void reset()
    {
        if coid_constexpr_if(TRACKING)
            mark_all_modified(false);

        //destroy occupied slots
        if coid_constexpr_if(!POOL) {
            destruct();
            extarray_reset();
        }
        else
            extarray_reset_count();

        _count = 0;

        _allocated.set_size(0);
    }

    ///Discard content. Also destroys pooled objects and frees memory
    void discard()
    {
        //destroy occupied slots
        destruct();

        extarray_discard();

        _count = 0;
        _allocated.discard();

        if coid_constexpr_if (LINEAR) {
            this->_array.discard();
        }
        else {
            this->_pages.discard();
            this->_created = 0;
        }
    }

#ifdef COID_CONSTEXPR_IF

protected:

    /// @{ versioning functions
    versionid get_versionid(uints id) const {
        DASSERT_RET(id <= versionid::max_id, versionid());
        if coid_constexpr_if (VERSIONING) {
            return versionid(uint(id), tracker_t::version_array()[id]);
        }
        else {
            return versionid(uint(id), 0);
        }
    }

    bool check_versionid(versionid vid) const {
        if coid_constexpr_if (VERSIONING) {
            if (!vid.is_valid() || vid.idx >= created())
                return false;
            uint8 ver = tracker_t::version_array()[vid.idx];
            return vid.version == ver;
        }
        else
            return true;
    }

    void bump_version(uints id) {
        if coid_constexpr_if (VERSIONING)
            ++tracker_t::version_array()[id];
    }

    /// @}

protected:

    /// @{ tracking functions

    void set_modified(uints k) const
    {
        if coid_constexpr_if (TRACKING) {
            //current frame is always at bit position 0
            dynarray<slotalloc_detail::changeset>& mods = const_cast<dynarray<slotalloc_detail::changeset>&>(
                std::get<sizeof...(Es)>(this->_exts));
            mods[k].mask |= 1;
        }
    }

    /// @}

protected:

    template<class...Ps>
    static T* construct_object(T* p, bool isold, Ps&&... ps) {
        if coid_constexpr_if (POOL) {
            if (isold) {
                //only in pool mode on reused objects, when someone calls push_construct
                //this is not a good usage pattern as it cannot reuse existing storage of the old object
                // (which is what pool mode is about)
                p->~T();
            }
        }

        return new(p) T(std::forward<Ps>(ps)...);
    }

    static T* copy_object(T* p, bool isold, const T& v) {
        if coid_constexpr_if (!POOL) {
            return new(p) T(v);
        }
        else {
            if (isold)
                *p = v;
            else
                new(p) T(v);

            return p;
        }
    }

    static T* copy_object(T* p, bool isold, T&& v) {
        if coid_constexpr_if (!POOL) {
            return new(p) T(std::forward<T>(v));
        }
        else {
            if (isold)
                *p = std::move(v);
            else
                new(p) T(std::forward<T>(v));

            return p;
        }
    }

#else

    static T* copy_object(T* dst, bool isold, const T& v) {
        return slotalloc_detail::constructor<POOL, T>::copy_object(dst, isold && POOL, v);
    }

    static T* copy_object(T* dst, bool isold, T&& v) {
        return slotalloc_detail::constructor<POOL, T>::copy_object(dst, isold && POOL, std::forward<T>(v));
    }

    template<class...Ps>
    static T* construct_object(T* dst, bool isold, Ps&&... ps) {
        return slotalloc_detail::constructor<POOL, T>::construct_object(dst, isold && POOL, std::forward<Ps>(ps)...);
    }

#endif //#ifdef COID_CONSTEXPR_IF

    static T* construct_default(T* p, bool isold) {
        if coid_constexpr_if(POOL) {
            return isold
                ? p
                : new(p) T;
        }

        return new(p) T;
    }

protected:
    template <class Te>
    static Te* construct_default_extarray_value(Te * p, bool isold) {
        if coid_constexpr_if(POOL) {
            return isold
                ? p
                : new(p) Te;
        }

        return new(p) Te;
    }

    template <class Te>
    static void destruct_extarray_value(Te* p) {
        p->~Te();
    }

    void destruct()
    {
        for_each([](T& v) { destroy(v); });

        if coid_constexpr_if (LINEAR)
            this->_array.set_size(0);
        else
            this->_created = 0;
    }


    /// @{Helper functions for for_each to allow calling with optional index argument
    template<class Fn>
    using has_index = std::integral_constant<bool, !(closure_traits<Fn>::arity::value <= 1)>;

    template<class Fn>
    using returns_void = std::integral_constant<bool, closure_traits<Fn>::returns_void::value>;

#ifdef COID_CONSTEXPR_IF
    template<class Fn>
    bool funccall_if(Fn fn, T& v, uints index) const {
        bool rv;
        if coid_constexpr_if (has_index<Fn>::value)
            rv = fn(v, index);
        else
            rv = fn(v);

        if coid_constexpr_if (TRACKING)
            set_modified(index);

        return rv;
    }

    template<class Fn>
    void funccallp(Fn fn, T* v, uints index) const {
        if coid_constexpr_if (has_index<Fn>::value)
            fn(v, index);
        else
            fn(v);
    }

    template<class Fn, class K = T>
    bool funccall(Fn fn, K& v, uints index) const {
        bool rv = true;
        if coid_constexpr_if (returns_void<Fn>::value) {
            if coid_constexpr_if (has_index<Fn>::value)
                fn(v, index);
            else
                fn(v);
        }
        else {
            if coid_constexpr_if (has_index<Fn>::value)
                rv = static_cast<bool>(fn(v, index));
            else
                rv = static_cast<bool>(fn(v));
        }

        if coid_constexpr_if (TRACKING) {
            if (rv)
                set_modified(index);
        }

        return rv; // && TRACKING;
    }

#else
    template<class Fn>
    using arg0 = typename std::remove_reference<typename closure_traits<Fn>::template arg<0>>::type;

    template<class Fn>
    using arg0ref = typename coid::nonptr_reference<typename closure_traits<Fn>::template arg<0>>::type;

    template<class Fn>
    using arg0constref = typename coid::nonptr_reference<const typename closure_traits<Fn>::template arg<0>>::type;

    template<class Fn>
    using is_const = std::is_const<arg0<Fn>>;

    ///A tracking void fnc(T&)
    template<typename Fn, typename = std::enable_if_t<!has_index<Fn>::value>>
    bool funccall_if(const Fn& fn, arg0ref<Fn> v, size_t index) const
    {
        bool ret = fn(v);
        this->set_modified(index);

        return ret;
    }

    ///A tracking void fnc(T&, index)
    template<typename Fn, typename = std::enable_if_t<has_index<Fn>::value>>
    bool funccall_if(const Fn& fn, arg0ref<Fn> v, const size_t& index) const
    {
        bool ret = fn(v, index);
        this->set_modified(index);

        return ret;
    }



    ///A non-tracking void fnc(const T&) const
    template<typename Fn, typename = std::enable_if_t<is_const<Fn>::value && !has_index<Fn>::value>>
    bool funccall(const Fn& fn, arg0constref<Fn> v, size_t&& index) const
    {
        fn(v);
        return false;
    }

    ///A tracking void fnc(T&)
    template<typename Fn, typename = std::enable_if_t<!is_const<Fn>::value && !has_index<Fn>::value && returns_void<Fn>::value>>
    bool funccall(const Fn& fn, arg0ref<Fn> v, const size_t&& index) const
    {
        fn(v);
        this->set_modified(index);

        return TRACKING;
    }

    ///Conditionally tracking bool fnc(T&)
    template<typename Fn, typename = std::enable_if_t<!is_const<Fn>::value && !has_index<Fn>::value && !returns_void<Fn>::value>>
    bool funccall(const Fn& fn, arg0ref<Fn> v, size_t&& index) const
    {
        bool rval = static_cast<bool>(fn(v));
        if (rval)
            this->set_modified(index);

        return rval;// && TRACKING;
    }

    ///A non-tracking void fnc(const T&, index) const
    template<typename Fn, typename = std::enable_if_t<is_const<Fn>::value && has_index<Fn>::value>>
    bool funccall(const Fn& fn, arg0constref<Fn> v, size_t index) const
    {
        fn(v, index);
        return false;
    }

    ///A tracking void fnc(T&, index)
    template<typename Fn, typename = std::enable_if_t<!is_const<Fn>::value && has_index<Fn>::value && returns_void<Fn>::value>>
    bool funccall(const Fn& fn, arg0ref<Fn> v, const size_t& index) const
    {
        fn(v, index);
        this->set_modified(index);

        return TRACKING;
    }

    ///Conditionally tracking bool fnc(T&, index)
    template<typename Fn, typename = std::enable_if_t<!is_const<Fn>::value && has_index<Fn>::value && !returns_void<Fn>::value>>
    bool funccall(const Fn& fn, arg0ref<Fn> v, size_t index) const
    {
        bool rval = static_cast<bool>(fn(v, index));
        if (rval)
            this->set_modified(index);

        return rval && TRACKING;
    }


    ///func call for for_each_modified (with ptr argument, 0 for deleted objects)
    template<typename Fn, typename = std::enable_if_t<!has_index<Fn>::value>>
    void funccallp(const Fn& fn, T* v, const size_t& index) const
    {
        fn(v);
    }

    template<typename Fn, typename = std::enable_if_t<has_index<Fn>::value>>
    void funccallp(const Fn& fn, T* v, size_t index) const
    {
        fn(v, index);
    }
#endif
    /// @}

public:

    ///Invoke a functor on each used item.
    /// @param f functor with ([const] T&) or ([const] T&, size_t index) arguments
    template<typename Func>
    void for_each(Func f) const
    {
        if coid_constexpr_if (LINEAR) {
            typedef std::remove_reference_t<typename closure_traits<Func>::template arg<0>> Tx;
            Tx* d = const_cast<Tx*>(this->_array.ptr());
            bitmask_type const* b = const_cast<bitmask_type const*>(_allocated.ptr());
            bitmask_type const* e = const_cast<bitmask_type const*>(_allocated.ptre());
            uints s = 0;

            for (bitmask_type const* p = b; p != e; ++p, s += BITMASK_BITS) {
                if (*p == 0)
                    continue;

                uints m = 1;
                for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                    if (*p & m)
                        funccall(f, d[s + i], s + i);
                    else if ((*p & ~(m - 1)) == 0)
                        break;
                }
            }
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            bitmask_type const* bm = const_cast<bitmask_type const*>(_allocated.ptr());
            bitmask_type const* em = const_cast<bitmask_type const*>(_allocated.ptre());
            bitmask_type const* pm = bm;
            uints gbase = 0;

            for (uints ip = 0; ip < this->_pages.size(); ++ip, gbase += page::ITEMS)
            {
                const page& pp = this->_pages[ip];
                T* data = const_cast<T*>(pp.ptr());

                bitmask_type const* epm = em - pm > page::NMASK
                    ? pm + page::NMASK
                    : em;

                uints pbase = 0;

                for (; pm != epm; ++pm, pbase += BITMASK_BITS) {
                    if (*pm == 0)
                        continue;

                    uints m = 1;
                    for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                        if (*pm & m)
                            funccall(f, data[pbase + i], gbase + pbase + i);
                        else if ((*pm & ~(m - 1)) == 0)
                            break;

                        //update after rebase
                        ints diffm = (ints)const_cast<bitmask_type const*>(_allocated.ptr()) - (ints)bm;
                        if (diffm) {
                            bm = ptr_byteshift(bm, diffm);
                            em = const_cast<bitmask_type const*>(_allocated.ptre());
                            pm = ptr_byteshift(pm, diffm);
                            epm = ptr_byteshift(epm, diffm);
                        }
                    }
                }
            }
        }
    }

    ///Invoke a functor on each used item in given ext array
    /// @note handles array insertions/deletions during iteration
    /// @param K ext array id
    /// @param f functor with ([const] T&) or ([const] T&, size_t index) arguments, with T being the type of given value array
    template<int K, typename Func>
    void for_each_in_array(Func f) const
    {
        auto extarray = value_array<K>().ptr();

        bitmask_type const* bm = const_cast<bitmask_type const*>(_allocated.ptr());
        bitmask_type const* em = const_cast<bitmask_type const*>(_allocated.ptre());
        uints s = 0;

        for (bitmask_type const* pm = bm; pm != em; ++pm, s += BITMASK_BITS) {
            if (*pm == 0)
                continue;

            uints m = 1;
            for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                if (*pm & m)
                    funccall(f, extarray[s + i], s + i);
                else if ((*pm & ~(m - 1)) == 0)
                    break;

                //update after rebase
                ints diffm = (ints)const_cast<bitmask_type const*>(_allocated.ptr()) - (ints)bm;
                if (diffm) {
                    bm = ptr_byteshift(bm, diffm);
                    em = const_cast<bitmask_type const*>(_allocated.ptre());
                    pm = ptr_byteshift(pm, diffm);
                }

                ints diffa = (ints)value_array<K>().ptr() - (ints)extarray;
                if (diffa)
                    extarray = ptr_byteshift(extarray, diffa);
            }
        }
    }

    ///Invoke a functor on each item that was modified between two frames
    /// @note const version doesn't handle array insertions/deletions during iteration
    /// @param bitplane_mask changeset bitplane mask (slotalloc_detail::changeset::bitplane_mask)
    /// @param f functor with ([const] T* ptr) or ([const] T* ptr, size_t index) arguments; ptr can be null if item was deleted
    template<typename Func>
    void for_each_modified(uint bitplane_mask, Func f) const COID_REQUIRES((TRACKING))
    {
        const bool all_modified = bitplane_mask > slotalloc_detail::changeset::BITPLANE_MASK;

        bitmask_type const* bm = const_cast<bitmask_type const*>(_allocated.ptr());
        bitmask_type const* em = const_cast<bitmask_type const*>(_allocated.ptre());

        auto chs = tracker_t::get_changeset();
        DASSERT(chs->size() >= uints(em - bm));

        const changeset_t* bc = chs->ptr();
        const changeset_t* ec = chs->ptre();
        if coid_constexpr_if (LINEAR) {
            T* pd0 = const_cast<T*>(this->_array.ptr());
            bitmask_type const* pm = bm;

            for (const changeset_t* ch = bc; ch < ec; ++pm) {
                uints m = pm < em ? *pm : 0U;
                uints idbase = (pm - bm) * BITMASK_BITS;

                for (int i = 0; ch < ec && i < BITMASK_BITS; ++i, m >>= 1, ++ch) {
                    if (all_modified || (ch->mask & bitplane_mask) != 0) {
                        T* pd = (m & 1) != 0 ? pd0 + (idbase + i) : 0;
                        funccallp(f, pd, idbase + i);
                    }
                }
            }
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            const page* bp = this->_pages.ptr();
            const page* ep = this->_pages.ptre();

            bitmask_type const* pm = bm;
            changeset_t const* pc = bc;
            uints gbase = 0;

            for (const page* pp = bp; pp < ep; ++pp, gbase += page::ITEMS)
            {
                T* data = const_cast<T*>(pp->data);
                changeset_t const* epc = ec - pc > page::NMASK
                    ? pc + page::ITEMS
                    : ec;

                uints pbase = 0;

                for (; pc < epc; ++pm, pbase += BITMASK_BITS) {
                    uints m = pm < em ? *pm : 0U;

                    for (int i = 0; pc < epc && i < BITMASK_BITS; ++i, m >>= 1, ++pc) {
                        if (all_modified || (pc->mask & bitplane_mask) != 0) {
                            T* pd = (m & 1) != 0 ? (T*)(data + pbase + i) : nullptr;
                            funccallp(f, pd, gbase + pbase + i);
                        }
                    }
                }
            }
        }
    }

    ///Run f(T*, [uints]) on a range of items
    /// @note this function ignores whether the items in range are allocated or not
    template<typename Func>
    void for_range_unchecked(uints id, uints count, Func f)
    {
        DASSERT_RET(id + count <= created());

        if coid_constexpr_if (LINEAR) {
            T* p = this->_array.ptr() + id;
            uints n = this->_array.size();
            if (id + count < n)
                n = id + count;

            for (uints i = 0; i < count; ++i)
                funccallp(f, p + i, id + i);
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            uint pg = uint(id / page::ITEMS);
            uint s = uint(id % page::ITEMS);
            uints i = id;

            while (count > 0) {
                T* b = this->_pages[pg++].ptr() + s;
                uints na = stdmin(page::ITEMS - s, count);
                T* e = b + na;

                for (; b < e; ++b, ++i)
                    funccallp(f, b, i);

                count -= na;
                s = 0;
            }
        }
    }

    ///Find first element for which the predicate returns true
    /// @return pointer to the element or null
    /// @param f functor with ([const] T&) or ([const] T&, size_t index) arguments
    template<typename Func>
    T* find_if(Func f) const
    {
        bitmask_type const* bm = const_cast<bitmask_type const*>(_allocated.ptr());
        bitmask_type const* em = const_cast<bitmask_type const*>(_allocated.ptre());

        if coid_constexpr_if (LINEAR) {
            uints base = 0;
            T* pd0 = const_cast<T*>(this->_array.ptr());

            for (bitmask_type const* pm = bm; pm != em; ++pm, base += BITMASK_BITS) {
                if (*pm == 0)
                    continue;

                uints m = 1;
                for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                    if (*pm & m) {
                        uints id = base + i;
                        if (funccall(f, pd0[id], base + i))
                            return pd0 + id;

                        //update after rebase
                        ints diffm = (ints)const_cast<bitmask_type const*>(_allocated.ptr()) - (ints)bm;
                        if (diffm) {
                            bm = ptr_byteshift(bm, diffm);
                            em = const_cast<bitmask_type const*>(_allocated.ptre());
                            pm = ptr_byteshift(pm, diffm);
                        }
                    }
                    else if ((*pm & ~(m - 1)) == 0)
                        break;
                }
            }
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            bitmask_type const* pm = bm;
            uints gbase = 0;

            for (uints ip = 0; ip < this->_pages.size(); ++ip, gbase += page::ITEMS)
            {
                const page& pp = this->_pages[ip];
                T* data = const_cast<T*>(pp.ptr());

                bitmask_type const* epm = em - pm > page::NMASK
                    ? pm + page::NMASK
                    : em;

                uints pbase = 0;

                for (; pm != epm; ++pm, pbase += BITMASK_BITS) {
                    if (*pm == 0)
                        continue;

                    uints m = 1;
                    for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                        if (*pm & m) {
                            if (funccall_if(f, data[pbase + i], gbase + pbase + i))
                                return const_cast<T*>(data) + (pbase + i);

                            //update after rebase
                            ints diffm = (ints)const_cast<bitmask_type const*>(_allocated.ptr()) - (ints)bm;
                            if (diffm) {
                                bm = ptr_byteshift(bm, diffm);
                                em = const_cast<bitmask_type const*>(_allocated.ptre());
                                pm = ptr_byteshift(pm, diffm);
                                epm = ptr_byteshift(epm, diffm);
                            }
                        }
                        else if ((*pm & ~(m - 1)) == 0)
                            break;
                    }
                }
            }
        }

        return 0;
    }

    ///Remove each element for which the predicate returns true
    /// @param f functor with ([const] T&) or ([const] T&, size_t index) arguments
    /// @return true if some item was deleted otherwise false
    template<typename Func>
    bool del_item_if(Func f)
    {
        bool found = false;
        bitmask_type const* bm = const_cast<bitmask_type const*>(_allocated.ptr());
        bitmask_type const* em = const_cast<bitmask_type const*>(_allocated.ptre());

        if coid_constexpr_if(LINEAR) {
            uints base = 0;
            T* pd0 = const_cast<T*>(this->_array.ptr());

            for (bitmask_type const* pm = bm; pm != em; ++pm, base += BITMASK_BITS) {
                if (*pm == 0)
                    continue;

                uints m = 1;
                for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                    if (*pm & m) {
                        uints id = base + i;
                        if (funccall(f, pd0[id], base + i))
                        {
                            del_item(pd0 + id);
                            found = true;
                        }

                        //update after rebase
                        ints diffm = (ints)const_cast<bitmask_type const*>(_allocated.ptr()) - (ints)bm;
                        if (diffm) {
                            bm = ptr_byteshift(bm, diffm);
                            em = const_cast<bitmask_type const*>(_allocated.ptre());
                            pm = ptr_byteshift(pm, diffm);
                        }
                    }
                    else if ((*pm & ~(m - 1)) == 0)
                        break;
                }
            }
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            bitmask_type const* pm = bm;
            uints gbase = 0;

            for (uints ip = 0; ip < this->_pages.size(); ++ip, gbase += page::ITEMS)
            {
                const page& pp = this->_pages[ip];
                T* data = const_cast<T*>(pp.ptr());

                bitmask_type const* epm = em - pm > page::NMASK
                    ? pm + page::NMASK
                    : em;

                uints pbase = 0;

                for (; pm != epm; ++pm, pbase += BITMASK_BITS) {
                    if (*pm == 0)
                        continue;

                    uints m = 1;
                    for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                        if (*pm & m) {
                            if (funccall_if(f, data[pbase + i], gbase + pbase + i))
                            {
                                del_item_by_ptr(const_cast<T*>(data) + (pbase + i));
                                found = true;
                            }

                            //update after rebase
                            ints diffm = (ints)const_cast<bitmask_type const*>(_allocated.ptr()) - (ints)bm;
                            if (diffm) {
                                bm = ptr_byteshift(bm, diffm);
                                em = const_cast<bitmask_type const*>(_allocated.ptre());
                                pm = ptr_byteshift(pm, diffm);
                                epm = ptr_byteshift(epm, diffm);
                            }
                        }
                        else if ((*pm & ~(m - 1)) == 0)
                            break;
                    }
                }
            }
        }

        return found;
    }

    ///Run on unused elements (freed elements in pool mode) until predicate returns true
    /// @return pointer to the element or null
    /// @param f functor with ([const] T&) or ([const] T&, size_t index) arguments
    template<typename Func>
    uints find_unused(Func f) const
    {
        if coid_constexpr_if(!POOL)
            return UMAXS;

        bitmask_type const* bm = const_cast<bitmask_type const*>(_allocated.ptr());
        bitmask_type const* em = const_cast<bitmask_type const*>(_allocated.ptre());
        bitmask_type const* pm = bm;

        if coid_constexpr_if (LINEAR) {
            uints pbase = 0;
            T* pd0 = const_cast<T*>(this->_array.ptr());
            uints n = this->_array.size();

            for (; pm != em; ++pm, pbase += BITMASK_BITS) {
                if (*pm == UMAXS)
                    continue;

                uints m = 1;
                for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                    uints id = pbase + i;
                    if (id >= n)
                        break;

                    if (!(*pm & m)) {
                        if (funccall_if(f, pd0[id], id))
                            return id;
                    }
                    //else if ((*pm & ~(m - 1)) == 0)
                    //    break;
                }
            }
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            const page* pb = this->_pages.ptr();
            const page* pe = this->_pages.ptre();

            bitmask_type const* pm = bm;
            uints gbase = 0;

            for (const page* pp = pb; pp < pe; ++pp, gbase += page::ITEMS)
            {
                T* d = const_cast<T*>(pp->ptr());
                bitmask_type const* epm = em - pm > page::NMASK
                    ? pm + page::NMASK
                    : em;

                uints pbase = 0;

                for (; pm != epm; ++pm, pbase += BITMASK_BITS) {
                    if (*pm == UMAXS)
                        continue;

                    uints m = 1;
                    for (int i = 0; i < BITMASK_BITS; ++i, m <<= 1) {
                        uints id = gbase + pbase + i;
                        if (id >= this->_created)
                            break;

                        if (!(*pm & m)) {
                            if (funccall_if(f, d[pbase + i], id))
                                return id;
                        }
                        //else if ((*pm & ~(m - 1)) == 0)
                        //    break;
                    }
                }
            }
        }

        return UMAXS;
    }

    /// @return bit array with marked item allocations
    const dynarray<uints>& get_bitarray() const { return _allocated; }

    ///Advance to the next tracking frame, updating changeset
    /// @return new frame number
    uint advance_frame()
    {
        if (!TRACKING)
            return 0;

        auto changeset = tracker_t::get_changeset();
        uint* frame = tracker_t::get_frame();

        update_changeset(*frame, *changeset);

        return ++ * frame;
    }

    ///Mark all objects that have the corresponding bit set as modified in current frame
    /// @param clear_old if true, old change bits are cleared
    void mark_all_modified(bool clear_old)
    {
        if (!TRACKING)
            return;

        bitmask_type const* b = const_cast<bitmask_type const*>(_allocated.ptr());
        bitmask_type const* e = const_cast<bitmask_type const*>(_allocated.ptre());
        bitmask_type const* p = b;

        const uint16 preserve = clear_old ? 0xfffeU : 0U;

        auto chs = tracker_t::get_changeset();
        DASSERT(chs->size() >= uints(e - b));

        changeset_t* chb = chs->ptr();
        changeset_t* che = chs->ptre();

        for (changeset_t* ch = chb; ch < che; p += BITMASK_BITS) {
            uints m = p < e ? uints(*p) : 0U;

            for (int i = 0; ch < che && i < BITMASK_BITS; ++i, m >>= 1, ++ch)
                ch->mask = (ch->mask & preserve) | (m & 1);
        }
    }

    struct iterator
    {
        typedef std::forward_iterator_tag iterator_category;

        iterator(base_t* base, bitmask_type index) : base(base), index(index)
        {}

        iterator& operator ++ () {
            index = base->next_index(index);
            return *this;
        }

        iterator operator ++ (int) const {
            return iterator(base, base->next_index(index));
        }

        bool operator == (const iterator& o) const {
            return base == o.base && index == o.index;
        }

        const T& operator * () const { return *base->get_item(index); }
        T& operator * () { return *base->get_item(index); }

        const T& operator -> () const { return *base->get_item(index); }
        T& operator -> () { return *base->get_item(index); }

    private:

        base_t* base = 0;
        uints index = uints(-1);
    };

    iterator begin() const {
        const uints first_id = is_valid_id(0) ? uints(0) : (this->allocated_count() == 0 ? uints(-1) : next_index(0));
        return iterator(const_cast<base_t*>(this), first_id);
    }

    iterator end() const {
        return iterator(const_cast<base_t*>(this), uints(-1));
    }

protected:

    uints next_index(bitmask_type index) const
    {
        using bitmask_value_type = typename storage_t::value_type;
        constexpr int mask_bits = (int)constexpr_int_high_pow2(BITMASK_BITS);
        constexpr uint bitmask_value_mask = (1 << mask_bits) - 1;

        uints id = index;
        ++id;
        uint bit = id & bitmask_value_mask;
        uints slot = id >> mask_bits;

        uints count = _allocated.size();
        while (slot < count) {
            bitmask_value_type mask = _allocated[slot];
            mask >>= bit;

            if (mask) {
                bit += lsb_bit_set(mask);
                return (slot << mask_bits) + bit;
            }

            bit = 0;
            ++slot;
        }

        return -1;
    }

    /// @return allocated and previously created count (not necessarily used currently)
    uints created() const {
        if coid_constexpr_if(LINEAR)
            return this->_array.size();
        else
            return this->_created;
    }

    template <class Te, class bitmask_type>
    static void copy_array_with_bitmask(const dynarray<Te>& src, dynarray<Te>& dst, const dynarray<bitmask_type>& mask)
    {
        uints rsv = src.reserved_virtual();
        if (rsv > 0)
            dst.reserve(rsv, reserve_mode::virtual_space);
        else
            dst.reserve(src.reserved_total(), reserve_mode::memory);
        Te* dd = dst.add_uninit(src.size());
        const Te* sd = src.ptr();
        constexpr int n = sizeof(bitmask_type) * 8;

        for (bitmask_type m : mask) {
            uint i = 0;
            while (m != 0) {
                if (m & 1)
                    new (dd + i) Te(sd[i]);
                ++i;
                m >>= 1;
            }
            dd += n;
            sd += n;
        }
    }

    void del_item_internal(T* p, uints id)
    {
        if (id >= created())
            throw exception("attempting to delete an invalid object ") << id;

        DASSERT_RET(get_bit(id));

        this->set_modified(id);

        //bump version on deletion, but not for pooled items that may be resurrected yet
        if coid_constexpr_if(!POOL)
            this->bump_version(id);

        if (clear_bit(id))
            --_count;
        else
            DASSERTN(0);

        if coid_constexpr_if(!POOL)
        {
            p->~T();
            extarray_destruct(id);
        }
    }

private:

    dynarray<bitmask_type> _allocated;      //< bit mask for allocated/free items

    bitmask_type _count = 0;                       //< active element count

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#endif

    ///Helper to expand all ext arrays
    template<size_t... Index>
    void extarray_expand_(index_sequence<Index...>, uints n) {
        int dummy[] = {0, ((void)std::get<Index>(this->_exts).add(n), 0)...};
    }

    void extarray_expand(uints n) {
        extarray_expand_(make_index_sequence<tracker_t::extarray_size>(), n);
    }

    ///Helper to expand all ext arrays
    template<size_t... Index>
    void extarray_expand_uninit_(index_sequence<Index...>, uints n) {
        int dummy[] = {0, ((void)std::get<Index>(this->_exts).add_uninit(n), 0)...};
    }

    void extarray_expand_uninit(uints n = 1) {
        extarray_expand_uninit_(make_index_sequence<tracker_t::extarray_size>(), n);
    }

    ///Helper to reset all ext arrays
    template<size_t... Index>
    void extarray_reset_(index_sequence<Index...>) {
        int dummy[] = {0, ((void)std::get<Index>(this->_exts).reset(), 0)...};
    }

    void extarray_reset() {
        extarray_reset_(make_index_sequence<tracker_t::extarray_size>());
    }

    ///Helper to set_count(0) all ext arrays
    template<size_t... Index>
    void extarray_reset_count_(index_sequence<Index...>) {
        int dummy[] = {0, ((void)std::get<Index>(this->_exts).set_size(0), 0)...};
    }

    void extarray_reset_count() {
        extarray_reset_count_(make_index_sequence<tracker_t::extarray_size>());
    }

    ///Helper to discard all ext arrays
    template<size_t... Index>
    void extarray_discard_(index_sequence<Index...>) {
        int dummy[] = {0, ((void)std::get<Index>(this->_exts).discard(), 0)...};
    }

    void extarray_discard() {
        extarray_discard_(make_index_sequence<tracker_t::extarray_size>());
    }

    ///Helper to reserve all ext arrays
    template<size_t... Index>
    void extarray_reserve_(index_sequence<Index...>, uints size) {
        int dummy[] = {0, ((void)std::get<Index>(this->_exts).reserve(size, true), 0)...};
    }

    template<size_t... Index>
    void extarray_reserve_virtual_(index_sequence<Index...>, uints size) {
        int dummy[] = {0, ((void)std::get<Index>(this->_exts).reserve_virtual(size), 0)...};
    }

    void extarray_reserve(uints size, reserve_mode mode) {
        if (mode == reserve_mode::virtual_space)
            extarray_reserve_virtual_(make_index_sequence<tracker_t::extarray_size>(), size);
        else
            extarray_reserve_(make_index_sequence<tracker_t::extarray_size>(), size);
    }


    ///Helper to iterate over all ext arrays
    template<size_t... Index>
    void extarray_copy_(const slotalloc_base& o, index_sequence<Index...>) {
        int dummy[] = {0, ((void)copy_array_with_bitmask(std::get<Index>(o._exts), std::get<Index>(this->_exts), _allocated), 0)...};
    }

    void extarray_copy(const slotalloc_base& o) {
        extarray_copy_(o, make_index_sequence<tracker_t::extarray_size>());
    }


    ///Helper to iterate over all ext arrays
    template<typename F, size_t... Index>
    void extarray_iterate_(index_sequence<Index...>, F fn) {
        int dummy[] = {0, ((void)fn(std::get<Index>(this->_exts)), 0)...};
    }

    template<typename F>
    void extarray_iterate(F fn) {
        extarray_iterate_(make_index_sequence<tracker_t::extarray_size>(), fn);
    }

    ///Helper to iterate over all ext arrays
    template<size_t... Index>
    void extarray_construct_default_(index_sequence<Index...>, uints id, bool is_old) {
        int dummy[] = { 0, (construct_default_extarray_value(std::get<Index>(this->_exts).ptr() + id, is_old), 0)...};
    }

    void extarray_construct_default(uints id, bool is_old) {
        extarray_construct_default_(make_index_sequence<tracker_t::extarray_size>(), id, is_old);
    }

    ///Helper to iterate over all ext arrays
    template<size_t... Index>
    void extarray_destruct_(index_sequence<Index...>, uints id) {
        int dummy[] = { 0, (destruct_extarray_value(std::get<Index>(this->_exts).ptr() + id), 0)... };
    }

    void extarray_destruct(uints id) {
        extarray_destruct_(make_index_sequence<tracker_t::extarray_size>(), id);
    }

#ifdef __clang__
#pragma clang diagnostic pop
#endif

    const T* ptr(uints id) const {
        if coid_constexpr_if (LINEAR)
            return this->_array.ptr() + id;
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            DASSERT(id / page::ITEMS < this->_pages.size());
            return (const T*)this->_pages[id / page::ITEMS].data + id % page::ITEMS;
        }
    }

    T* ptr(uints id) {
        if coid_constexpr_if (LINEAR)
            return this->_array.ptr() + id;
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            DASSERT(id / page::ITEMS < this->_pages.size());
            return (T*)this->_pages[id / page::ITEMS].data + id % page::ITEMS;
        }
    }

    ///Return allocated slot
    T* alloc(uints* pid)
    {
        DASSERT(_count < created());

        bitmask_type* p = _allocated.ptr();
        bitmask_type* e = _allocated.ptre();
        for (; p != e && *p == UMAXS; ++p);

        if (p == e)
            *(p = _allocated.add()) = 0;

        uint8 bit = lsb_bit_set((uints)~*p);
        uints slot = (p - _allocated.ptr()) * BITMASK_BITS;

        uints id = slot + bit;
        this->set_modified(id);

        DASSERT(!get_bit(id));

        if (pid)
            *pid = id;

        *p |= uints(1) << bit;
        ++_count;

        return ptr(id);
    }

    ///Return allocated slot
    T* alloc_item(uints id)
    {
        DASSERT(id < created());
        DASSERT(!get_bit(id));

        bitmask_type* p = &_allocated[id / BITMASK_BITS];
        uint8 bit = id & (BITMASK_BITS - 1);

        *p |= uints(1) << bit;
        ++_count;

        this->set_modified(id);

        return ptr(id);
    }

    ///Alloc range of objects, can cross page boundaries
    /// @param old receives number of reused objects lying at the beginning of the range
    template <bool UNINIT>
    uints alloc_range(uints n, uints* old)
    {
        uints id = find_zero_bitrange(n, _allocated.ptr(), _allocated.ptre());
        uints nslots = align_to_chunks(id + n, BITMASK_BITS);

        if (nslots > _allocated.size())
            _allocated.addc(nslots - _allocated.size());

        set_bitrange(id, n, _allocated.ptr());

        uints ncr = created();
        uints nadd = id + n > ncr ? id + n - ncr : 0;
        if (nadd)
            expand<UNINIT, UNINIT>(nadd);
        *old = n - nadd;

        _count += n;

        DASSERT(!TRACKING);
        return id;
    }

    ///Alloc range of objects within a single contiguous page
    /// @param old receives number of reused objects lying at the beginning of the range
    template <bool UNINIT>
    uints alloc_range_contiguous(uints n, uints* old)
    {
        if coid_constexpr_if (!LINEAR) {
            if (n > storage_t::page::ITEMS)
                return UMAXS;
        }

        bitmask_type const* bm = _allocated.ptr();
        bitmask_type const* em = _allocated.ptre();
        uints id = 0;
        if coid_constexpr_if (LINEAR) {
            id = find_zero_bitrange(n, bm, em);
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            page* ep = this->_pages.ptre();
            page* bp = this->_pages.ptr();
            page* pp = bp;

            bitmask_type const* pm = bm;

            for (; pp != ep; ++pp, pm += page::NMASK)
            {
                bitmask_type const* epm = em - pm > page::NMASK
                    ? pm + page::NMASK
                    : em;

                uints lid = find_zero_bitrange(n, pm, epm);
                if (lid + n <= page::ITEMS) {
                    id = lid + (pp - bp) * page::ITEMS;
                    break;
                }
            }

            if (pp == ep) {
                id = this->_pages.size() * page::ITEMS;

                void* oldbase = this->_pages.ptr();
                pp = this->_pages.add();

                if coid_constexpr_if(ATOMIC) {
                    DASSERT(oldbase == nullptr || oldbase == this->_pages.ptr()); // check if page dynarray rebased
                }
            }
        }

        uints nslots = align_to_chunks(id + n, BITMASK_BITS);

        if (nslots > _allocated.size())
            _allocated.addc(nslots - _allocated.size());

        set_bitrange(id, n, _allocated.ptr());

        uints ncr = created();
        uints nadd = id + n > ncr ? id + n - ncr : 0;
        if (nadd)
            expand<UNINIT,UNINIT>(nadd);
        *old = n - nadd;

        _count += n;

        DASSERT(!TRACKING);
        return id;
    }

    ///Append to a full array
    template <bool EXT_UNINIT>
    T* append(uints* pid = 0)
    {
        uints count = created();

        //DASSERT(_count <= count);   //count may be lower with other threads deleting, but not higher (single producer)
        set_bit(count);

        if (pid)
            *pid = count;
        ++_count;

        T* result = expand<true, EXT_UNINIT>(1);
        this->set_modified(count);

        return result;
    }

    template <bool UNINIT, bool EXT_UNINIT>
    T* expand(uints n)
    {
        uints base = created();

        if coid_constexpr_if (LINEAR) {
            ints rebase = 0;
            this->_array.add_uninit(n, &rebase);

            if (rebase)
                //note: an exception here means it's necessary to reserve items and/or to call allow_rebase()
                throw exception("a linear array rebased");
        }
        else {
            //using page = typename storage_t::page;
            typedef typename storage_t::page page;

            uints np = align_to_chunks(this->_created + n, page::ITEMS);
            if (np > this->_pages.size()) {
                void* oldbase = this->_pages.ptr();
                this->_pages.realloc(np);
                if coid_constexpr_if(ATOMIC) {
                    DASSERT(oldbase == nullptr || oldbase == this->_pages.ptr()); // check if page dynarray rebased
                }
            }

            this->_created += n;
        }

        // Expand ext arrays
        if coid_constexpr_if(UNINIT)
        {
            extarray_expand_uninit(n);
            if coid_constexpr_if(VERSIONING)
            {
                // always set zeros to version arrays even on uninit expand
                auto& version_arr = tracker_t::version_array();
                constexpr static uint8 version_type_byte_size = sizeof(std::remove_reference_t<decltype(version_arr)>::value_type);
                version_arr.memset(version_arr.ptre() - n, 0, n * version_type_byte_size);
            }
        }
        else
        {
            extarray_expand(n);
        }

        //in POOL mode the unallocated items in between the valid ones are assumed to be constructed
        if coid_constexpr_if(POOL) {
            if (!UNINIT && n > 1) {
                for_range_unchecked(base, n - 1, [](T* p) {
#ifdef COID_CONSTEXPR_IF
                    if coid_constexpr_if (!UNINIT)
                        new(p) T;
#else
                    slotalloc_detail::newtor<UNINIT, T>::create(p);
#endif
                    });
            }
        }

        T* p = ptr(created() - 1);

#ifdef COID_CONSTEXPR_IF
        if coid_constexpr_if(!UNINIT)
            return new(p) T;
        else
            return p;
#else
        return slotalloc_detail::newtor<UNINIT, T>::create(p);
#endif
    }

    bool set_bit(uints k) { return _allocated.set_bit(k); }
    bool clear_bit(uints k) { return _allocated.clear_bit(k); }
    bool get_bit(uints k) const { return _allocated.get_bit(k); }

    //WA for lambda template error
    void static destroy(T& p) { p.~T(); }


    static void update_changeset(uint frame, dynarray<changeset_t>& changeset)
    {
        //the changeset keeps n bits per each element, marking if there was a change in data
        // half of the bits correspond to the given number of most recent frames
        // older frames will be coalesced, containing flags that tell if there was a change in any of the
        // coalesced frames
        //frame aggregation:
        //      8844222211111111 (MSb to LSb)

        changeset_t* chb = changeset.ptr();
        changeset_t* che = changeset.ptre();

        //make space for a new frame

        bool b8 = (frame & 7) == 0;
        bool b4 = (frame & 3) == 0;
        bool b2 = (frame & 1) == 0;

        for (changeset_t* ch = chb; ch < che; ++ch) {
            uint16 v = ch->mask;
            uint16 vs = (v << 1) & 0xaeff;                 //shifted bits
            uint16 va = ((v << 1) | (v << 2)) & 0x5100;    //aggregated bits
            uint16 vx = vs | va;

            uint16 xc000 = (b8 ? vx : v) & (3 << 14);
            uint16 x3000 = (b4 ? vx : v) & (3 << 12);
            uint16 x0f00 = (b2 ? vx : v) & (15 << 8);
            uint16 x00ff = vs & 0xff;

            ch->mask = xc000 | x3000 | x0f00 | x00ff;
        }
    }
};

//variants of slotalloc

template<class T, class ...Es>
using slotalloc = slotalloc_base<T, slotalloc_mode::base, Es...>;

template<class T, class ...Es>
using slotalloc_linear = slotalloc_base<T, slotalloc_mode::linear, Es...>;

template<class T, class ...Es>
using slotalloc_versioning = slotalloc_base<T, slotalloc_mode::versioning, Es...>;

template<class T, class ...Es>
using slotalloc_versioning_linear = slotalloc_base<T, slotalloc_mode::versioning | slotalloc_mode::linear, Es...>;

//@{ pool variants

template<class T, class ...Es>
using slotalloc_pool = slotalloc_base<T, slotalloc_mode::pool, Es...>;

template<class T, class ...Es>
using slotalloc_atomic_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::atomic, Es...>;

template<class T, class ...Es>
using slotalloc_tracking_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::tracking, Es...>;

template<class T, class ...Es>
using slotalloc_tracking_atomic_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::atomic | slotalloc_mode::tracking, Es...>;


template<class T, class ...Es>
using slotalloc_versioning_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::versioning, Es...>;

template<class T, class ...Es>
using slotalloc_versioning_atomic_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::atomic | slotalloc_mode::versioning, Es...>;

//linear

template<class T, class ...Es>
using slotalloc_linear_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::linear, Es...>;

template<class T, class ...Es>
using slotalloc_atomic_linear_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::atomic | slotalloc_mode::linear, Es...>;

template<class T, class ...Es>
using slotalloc_tracking_linear_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::tracking | slotalloc_mode::linear, Es...>;

template<class T, class ...Es>
using slotalloc_tracking_atomic_linear_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::atomic | slotalloc_mode::tracking | slotalloc_mode::linear, Es...>;


template<class T, class ...Es>
using slotalloc_versioning_linear_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::versioning | slotalloc_mode::linear, Es...>;

template<class T, class ...Es>
using slotalloc_versioning_atomic_linear_pool = slotalloc_base<T, slotalloc_mode::pool | slotalloc_mode::atomic | slotalloc_mode::versioning | slotalloc_mode::linear, Es...>;
//@}

template<class T, class ...Es>
using slotalloc_atomic = slotalloc_base<T, slotalloc_mode::atomic, Es...>;

template<class T, class ...Es>
using slotalloc_tracking_atomic = slotalloc_base<T, slotalloc_mode::atomic | slotalloc_mode::tracking, Es...>;

template<class T, class ...Es>
using slotalloc_tracking = slotalloc_base<T, slotalloc_mode::tracking, Es...>;


template<class T, class ...Es>
using slotalloc_atomic_linear = slotalloc_base<T, slotalloc_mode::atomic | slotalloc_mode::linear, Es...>;

template<class T, class ...Es>
using slotalloc_tracking_atomic_linear = slotalloc_base<T, slotalloc_mode::atomic | slotalloc_mode::tracking | slotalloc_mode::linear, Es...>;

template<class T, class ...Es>
using slotalloc_tracking_linear = slotalloc_base<T, slotalloc_mode::tracking | slotalloc_mode::linear, Es...>;

COID_NAMESPACE_END
