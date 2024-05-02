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
 * Portions created by the Initial Developer are Copyright (C) 2021-2023
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


#include "namespace.h"
#include "typeseq.h"
#include "hash/slothash.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////

/// @brief class for fast access of heterogenous data as components
/// @note keeps T* in array, given T is always in the same slot across all instances of context_holder
/// @tparam OwnT owner class (or any type) used to store shared data
template <class OwnT>
class context_holder
{
public:

    /// @return reference to a component pointer
    /// @param pid [optional] receives component id
    template <class T>
    T*& component(int* pid = 0)
    {
        type_sequencer<OwnT>& sq = tsq();
        int id = sq.type_id<T>();
        if (pid)
            *pid = id;
        void*& ctx = _components.get_or_addc(id);
        return reinterpret_cast<T*&>(ctx);
    }

    /// @return const component pointer
    /// @param pid [optional] receives component id
    template <class T>
    const T* component(int* pid = 0) const
    {
        type_sequencer<OwnT>& sq = tsq();
        int id = sq.type_id<T>();
        if (pid)
            *pid = id;
        void* const& ctx = _components.get_safe(id, nullptr);
        return reinterpret_cast<const T*>(ctx);
    }

    /// @return component id (sequential by order of registration)
    template <class T>
    int component_id() {
        type_sequencer<OwnT>& sq = tsq();
        return sq.type_id<T>();
    }

    void* component_data(int cid) const {
        return cid < (int)_components.size() ? _components[cid] : nullptr;
    }

    void iterate_all_components(void (*fn)(void* component_ptr, uint component_id))
    {
        for (uint i = 0, e = _components.size(); i < e; ++i)
        {
            if (_components[i] != nullptr)
            {
                fn(_components[i], i);
            }
        }
    }

private:

    static type_sequencer<OwnT>& tsq() {
        LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(type_sequencer<OwnT>) _tsq = new type_sequencer<OwnT>;
        return *_tsq;
    }

    dynarray32<void*> _components;
};


////////////////////////////////////////////////////////////////////////////////

/// @brief Data manager template class
/// Keep mulitple dynamically created linear or slothash-ed containers, to which it provides a simple access via the data class type.
/// Containers are created automatically when pushing data.
/// e.g.
///     class entman : public data_manager<entman> {};
///     entman::push(id, type1());
///     entman::get<type2>();
///
/// @tparam OwnT owner class (or any type) used to store the shared data
template <class OwnT>
class data_manager
{
    struct erecord
    {
        uint16 version;
    };

    struct sequencer : type_sequencer<OwnT>
    {
        static constexpr int BITMASK_BITS = 8 * sizeof(uints);

        sequencer()
            : _allocated(coid::abyss_dynarray_size, coid::reserve_mode::virtual_space)
            , _entities(coid::abyss_dynarray_size, coid::reserve_mode::virtual_space)
        {}

        bool set_bit(uints k) { return _allocated.set_bit(k); }
        bool clear_bit(uints k) { return _allocated.clear_bit(k); }
        bool get_bit(uints k) const { return _allocated.get_bit(k); }

        erecord* alloc(uint* pid)
        {
            DASSERT(_count < _entities.size());

            uints* p = _allocated.ptr();
            uints* e = _allocated.ptre();
            for (; p != e && *p == UMAXS; ++p);

            if (p == e)
                *(p = _allocated.add()) = 0;

            uint8 bit = lsb_bit_set((uints)~*p);
            uint slot = uint((p - _allocated.ptr()) * BITMASK_BITS);
            uint id = slot + bit;

            DASSERT(!get_bit(id));

            if (pid)
                *pid = id;

            *p |= uints(1) << bit;
            ++_count;

            return &_entities[id];
        }

        erecord* append(uint* pid = 0)
        {
            uint count = _entities.size();
            set_bit(count);

            if (pid)
                *pid = count;
            ++_count;

            //alloc BITMASK_BITS entries at once to simplify validity checking
            erecord* p = _entities.addc(BITMASK_BITS);
            return p;
        }

        ///Add new object initialized with default constructor, or reuse one in pool mode
        erecord* add(uint* pid = 0)
        {
            bool isold = _count < _entities.size();
            return isold ? alloc(pid) : append(pid);
        }

        ///Delete object by id
        bool del(uint id)
        {
            DASSERT_RET(id < _entities.size(), false);

            if (clear_bit(id)) {
                ++_entities[id].version;
                --_count;
                return true;
            }

            DASSERTN(0);
            return false;
        }

        bool del(versionid vid)
        {
            DASSERT_RET(get_bit(vid.id) && _entities[vid.id].version == vid.version, false);
            if (clear_bit(vid.id)) {
                --_count;
                return true;
            }
            return false;
        }

        bool is_valid(uint id) const {
            return get_bit(id);
        }

        bool is_valid(versionid v) const {
            return get_bit(v.id) && _entities[v.id].version == v.version;
        }

        versionid get_versionid(uint gid) const {
            DASSERT_RET(get_bit(gid), versionid());
            return versionid(gid, _entities[gid].version);
        }

        dynarray32<uints> _allocated;
        dynarray32<erecord> _entities;
        uint _count = 0;
    };

    static sequencer& tsq() {
        LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(sequencer) _tsq = new sequencer;
        return *_tsq;
    }



    struct container
    {
        size_t element_size;
        sequencer* seq = 0;

        uint16 container_id = -1;

        enum class type : uint8 {
            array,
            hash,
        } storage_type;


        container(uint id, uint size, enum class type t) : element_size(size), container_id(id), storage_type(t) {
            seq = &tsq();
        }
        virtual ~container() = default;

        /// @brief Fetch element from container
        virtual void* element(uint gid) = 0;

        /// @brief  Fetch element by container local id (not a global one)
        /// @param id container local id
        virtual void* element_by_container_local_id(uint id) = 0;

        /// @brief Return global id from a pointer to element
        /// @param p pointer to container element data
        /// @return global id
        virtual uint element_id(const void* p) const = 0;

        /// @brief Create element
        /// @return nullptr if element already exists
        virtual void* create_default(uint gid) = 0;
        virtual void* create_uninit(uint gid) = 0;

        virtual void remove(uint gid) = 0;

        /// @return pointer to a dynarray with linear storage
        virtual const void* linear_array_ptr() const = 0;

        /// @brief Reserve memory
        /// @param count number of elements to reserve memory for
        virtual void reserve(uint count) = 0;

        virtual versionid allocate() = 0;

        /// @param gid global entity id
        /// @return local id of element in given container
        virtual uint get_container_local_item_id(uint gid) const = 0;

        virtual uint get_used_count() const = 0;
    };

    /// @brief helper class to hold entity id for hashed data containers
    template <class T> struct storage : T {
        uint eid;
    };

    /// @brief slothash type container for sparse data
    /// @tparam T
    template <class T>
    struct cshash : container
    {
        static constexpr container::type specific_type = container::type::hash;

        struct extractor {
            using value_type = storage<T>;
            uint operator()(const storage<T>& s) const { return s.eid; }
        };

        explicit cshash(uint id) : container(id, sizeof(storage<T>), container::type::hash) {
            hash.reserve_virtual(1 << 23);
        }
        void* element(uint gid) override final { return (T*)hash.find_value(gid); }
        void* element_by_container_local_id(uint id) override final {
            return hash.get_item(id);
        }

        uint element_id(const void* p) const {
            DASSERT(hash.is_valid(static_cast<const storage<T>*>(p)));
            return static_cast<const storage<T>*>(p)->eid;
        }

        void* create_default(uint gid) override final {
            if constexpr (std::is_default_constructible_v<T>) {
                void* p = create_uninit(gid);
                return p ? new (p) T : nullptr;
            }

            __assume(false);
        }
        void* create_uninit(uint gid) override final {
            bool is_new;
            storage<T>* item = hash.find_or_insert_value_slot_uninit(gid, &is_new);
            if (!is_new)
                return nullptr;
            item->eid = gid;
            return item;
        }
        void remove(uint gid) override final {
            hash.erase(gid);
        }
        const void* linear_array_ptr() const override final {
            return 0;   //not supported
        }
        void reserve(uint count) override final {
            hash.reserve_virtual(count);
        }
        versionid allocate() override final {
            DASSERT(0);
            return versionid();
        }

        uint get_container_local_item_id(uint gid) const override final
        {
            return coid::down_cast<uint>(hash.get_item_id(hash.find_value(gid)));
        }

        uint get_used_count() const override final
        {
            return coid::down_cast<uint>(hash.count());
        }

        slothash<storage<T>, uint, slotalloc_mode::versioning | slotalloc_mode::linear | slotalloc_mode::atomic, extractor> hash;
    };

    /// @brief linear array container
    /// @tparam T
    template <class T>
    struct carray : container
    {
        static constexpr container::type specific_type = container::type::array;

        explicit carray(uint id) : container(id, sizeof(T), container::type::array) {
            data.reserve(1 << 23);
        }
        void* element(uint gid) override final { return gid < data.size() ? &data[gid] : nullptr; }
        void* element_by_container_local_id(uint id) override final {
            return element(id);
        }
        uint element_id(const void* p) const {
            const T* pt = static_cast<const T*>(p);
            DASSERT(pt >= data.ptr() && pt < data.ptre());
            return uint(pt - data.ptr());
        }

        void* create_default(uint gid) override final {
            if constexpr (std::is_default_constructible_v<T>)
                return &data.get_or_add(gid);

            __assume(false);
        }
        void* create_uninit(uint gid) override final {
            uint n = data.size();
            if (gid < n) {
                //DASSERT(0);
                return &data[gid];  //not uninitialized but we can't call destructor
            }
            T* p = data.add_uninit(gid + 1 - n);
            return data.ptr() + gid;
        }
        void remove(uint gid) override final {
            DASSERT(0);
        }
        const void* linear_array_ptr() const override final {
            return &data;
        }
        void reserve(uint count) override final {
            data.reserve_virtual(count);
        }
        versionid allocate() override final {
            DASSERT(0);
            return versionid();
        }

        uint get_container_local_item_id(uint gid) const override final {
            //array uses global ids
            return gid;
        }

        uint get_used_count() const override final { return data.size(); }

        dynarray32<T> data;
    };

    // manipulators for the data

public:
    struct handler_data {
        virtual ~handler_data() {}
    };

private:
    template <class M>
    struct base_handler {
        virtual ~base_handler() {}
        virtual void invoke(M*, void*, uint) = 0;
    };

    template <class M, class C>
    struct handler : base_handler<M>
    {
        using handler_t = void (*)(M*, C&, uint, handler_data*);

        virtual void invoke(M* m, void* c, uint gid) override {
            return fn(m, *(C*)c, gid, data);
        }

        handler(handler_t&& fn, handler_data* data) {
            this->fn = std::move(fn);
            this->data = data;
        }

        ~handler() {
            delete data;
        }

        handler_t fn;
        handler_data* data = nullptr;
    };

    /// @brief Manipulator base class. Manipulators allow working with different components of the entity, e.g. invoke UI handlers for each component
    /// @tparam M manipulator specialization, e.g. class xyz : manipulator<xyz> ...
    template <class M>
    struct manipulator {
        virtual ~manipulator() {}
        coid::dynarray32<base_handler<M>*> component_handlers;
    };

public:

    /// @brief Get container id assigned to the type (also assigns on first use)
    /// @return -0-based type index
    template <class C>
    static int get_container_id()
    {
        return tsq().type_id<C>();
    }


    /// @brief Retrieve data of given type
    /// @tparam C data type
    /// @param vid versioned entity id
    /// @return data pointer
    static void* get_by_container_id(versionid vid, uint cid)
    {
        dynarray32<container*>& conts = data_containers();
        DASSERT_RET(cid < conts.size(), 0);

        container* c = conts[cid];
        if (!c || !c->seq->is_valid(vid))
            return nullptr;

        return c->element(vid.id);
    }



    /// @brief Retrieve data of given type
    /// @tparam C data type
    /// @param gid global entity id
    /// @return data pointer
    template <class C>
    static C* get(uint gid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();

        return c ? static_cast<C*>(c->element(gid)) : nullptr;
    }

    /// @brief Retrieve data of given type
    /// @tparam C data type
    /// @param vid versioned entity id
    /// @return data pointer
    template <class C>
    static C* get(versionid vid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();

        if (!c || !c->seq->is_valid(vid))
            return nullptr;

        return static_cast<C*>(c->element(vid.id));
    }

    template <class C>
    static C& get_or_create(versionid vid)
    {
        static container* c = 0;
        if (!c) c = &get_or_create_container<C>();

        C* el = static_cast<C*>(c->element(vid.id));
        if (!el)
            el = static_cast<C*>(c->create_default(vid.id));

        return *el;
    }

    /// @brief Retrieve data of given type
    /// @tparam C data type
    /// @param id container item id
    /// @return data pointer
    template <class C>
    static C* get_by_container_local_id(uint id)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();

        return c ? static_cast<C*>(c->element_by_container_local_id(id)) : nullptr;
    }

    static void* get_data(uint gid, uint c)
    {
        container* co = data_containers().get_safe(c, nullptr);
        if (!co)
            return 0;

        return co->element(gid);
    }

    /// @brief Create entity
    /// @return newely created entity id
    static versionid allocate()
    {
        static sequencer& seq = tsq();
        uint id;
        erecord* er = seq.add(&id);

        for (container* c : data_containers()) {
            if (c && c->storage_type == container::type::array)
                c->create_default(id);
        }

        return versionid(id, er->version);
    }

    /// @brief Free entity
    static void free(versionid vid) {
        static sequencer& seq = tsq();
        if (seq.del(vid)) {
            for (container* c : data_containers()) {
                if (c && c->storage_type == container::type::hash)
                    c->remove(vid.id);
            }
        }
    }

    /// @brief Free entity
    static void free(uint gid) {
        static sequencer& seq = tsq();
        if (seq.del(gid)) {
            for (container* c : data_containers()) {
                if (c && c->storage_type == container::type::hash)
                    c->remove(gid);
            }
        }
    }

    /// @brief Run destructor on component
    /// @tparam OwnT
    template <class C>
    static void destruct(uint gid) {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c);
        C* co = static_cast<C*>(c->element(gid));
        if (co)
            co->~C();
    }

    static bool is_valid(uint gid) {
        static sequencer& seq = tsq();
        return seq.is_valid(gid);
    }

    static bool is_valid(versionid vid) {
        static sequencer& seq = tsq();
        return seq.is_valid(vid);
    }

    /// @param gid global id (non-versioned)
    /// @return versioned id from given global id
    static versionid get_versionid(uint gid) {
        static sequencer& seq = tsq();
        return seq.get_versionid(gid);
    }

    /// @brief Insert data of given type under the entity id
    /// @tparam C data type
    /// @param vid entity handle
    /// @param v data value to move from
    /// @return pointer to the inserted data
    template <class C>
    static C* push(versionid vid, C&& v)
    {
        static container* c = &get_or_create_container<C, cshash<C>>();
        DASSERT_RET(c, nullptr);
        DASSERT_RET(c->seq->is_valid(vid), nullptr);

        C* d = static_cast<C*>(c->create_default(vid.id));
        *d = std::move(v);
        DASSERT(c->storage_type != container::type::hash || static_cast<storage<C>*>(d)->eid == vid.id);

        return d;
    }

    /// @brief Insert default constructed data
    /// @tparam C data type
    /// @param vid entity handle
    /// @return pointer to the uninitialized data
    template <class C>
    static C* push_default(versionid vid)
    {
        static container* c = &get_or_create_container<C, cshash<C>>();
        DASSERT_RET(c, nullptr);
        DASSERT_RET(c->seq->is_valid(vid), nullptr);

        return static_cast<C*>(c->create_default(vid.id));
    }

    /// @brief Insert uninitialized data, to be in-place constructed
    /// @tparam C data type
    /// @param vid entity handle
    /// @return pointer to the uninitialized data
    template <class C, class...Ps>
    static C* emplace(versionid vid, Ps&&... ps)
    {
        static container* c = &get_or_create_container<C, cshash<C>>();
        DASSERT_RET(c, nullptr);
        DASSERT_RET(c->seq->is_valid(vid.id), nullptr);

        void* p = c->create_uninit(vid.id);
        DASSERT(p);
        return new (p) C(std::forward<Ps>(ps)...);
    }

    /// @brief Remove component from entity
    /// @tparam C data type
    /// @param gid global entity id
    template <class C>
    static void remove(uint gid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c);

        c->remove(gid);
    }

    /// @brief Remove component from entity
    /// @tparam C data type
    /// @param p pointer to component
    template <class C>
    static void remove(C* p)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c);

        uint gid = c->element_id(p);
        c->remove(gid);
    }

    /// @brief Remove component from entity
    /// @tparam C data type
    /// @param vid versioned entity id
    template <class C>
    static void remove(versionid vid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c);

        DASSERT_RET(c->seq->is_valid(vid));
        c->remove(vid.id);
    }

    /// @brief Get container id of element of given type
    /// @tparam C Type of element
    /// @param gid global element id
    /// @return container id of given element
    /// @note Container local id is internal id of element in underlying container type( eg. index for arrays, slot_id for slothash)
    template <class C>
    static uint get_container_local_id(uint gid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c, -1);

        return c->get_container_local_item_id(gid);
    }

    /// @brief Get container id of element of given type
    /// @tparam C Type of element
    /// @param vid element version id
    /// @return container id of given element
    /// @note Container local id is internal id of element in underlying container type( eg. index for arrays, slot_id for slothash)
    template <class C>
    static uint get_container_local_id(versionid vid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c, -1);
        DASSERT_RET(c->seq->is_valid(vid), -1);

        return c->get_container_local_item_id(coid::down_cast<uint>(vid.id));
    }


    /// @brief Get local id of container element
    /// @tparam Type of element
    /// @param p pointer to container element
    /// @return local id
    template <class C>
    static uint get_container_local_id(const C* p)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c, -1);
        return c->get_container_local_item_id(c->element_id(p));
    }

    /// @brief Get global id of container element
    /// @tparam C Type of element
    /// @param p pointer to container element
    /// @return global id
    template <class C>
    static uint get_gid(const C* p) {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c, -1);

        return c->element_id(p);
    }

    /// @brief Get global id of container element
    /// @tparam C Type of element
    /// @param p pointer to container element
    /// @return global id
    template <class C>
    static versionid get_versionid(const C* p) {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c, versionid());

        return get_versionid(c->element_id(p));
    }

    /// @brief Get count of elements of type C
    /// @tparam C Type of element
    /// @return count
    template<class C>
    static uint get_count()
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c, -1);

        return c->get_used_count();
    }

    /// @brief Enumerate through entities with given component
    /// @tparam C data type
    /// @tparam Fn
    /// @param fn - void fn(C& component, uint id)
    template <class C, class Fn>
    static void enumerate(const Fn& fn)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        if (!c) return;

        if (c->storage_type == container::type::array) {
            //TODO: use bitmask
            carray<C>& ca = *static_cast<carray<C>*>(c);
            uint id = 0;
            for (C& e : ca.data) {
                fn(e, id++);
            }
        }
        else {
            cshash<C>& ch = *static_cast<cshash<C>*>(c);
            for (storage<C>& e : ch.hash) {
                fn((C&)e, e.eid);
            }
        }
    }

    /// @brief Define a primary data type, that will be stored in a linear array indexed directly
    /// @tparam C data type
    /// @param reserve_count number of elements to reserve initial memory for
    /// @return container id
    /// @note container id 0 is created as a slotalloc to keep track of existing and deleted objects
    template <class C>
    static void preallocate_array_container()
    {
        static sequencer& seq = tsq();
        DASSERT(seq._count == 0);
        carray<C>* cont = static_cast<carray<C>*>(&get_or_create_container<C, carray<C>>());
        //TODO: this function just ensures that container of type C exists, refactor candidate
        //cont->reserve(reserve_count);
    }

    template <class C>
    static void preallocate_hash_container()
    {
        cshash<C>* cont = static_cast<cshash<C>*>(&get_or_create_container<C, cshash<C>>());
        //TODO: this function just ensures that container of type C exists, refactor candidate
        //cont->reserve(reserve_count);
    }

    /// @brief Retrieve container for primary data
    /// @tparam C
    /// @return linear array of given data type, or nullptr if given storage doesn't have linear data
    template <class C>
    static const dynarray32<C>& get_array()
    {
        static const dynarray32<C>* c = static_cast<const dynarray32<C>*>(get_or_create_container<C, carray<C>>().linear_array_ptr());
        RASSERT(c != nullptr);

        return *c;
    }

    /// @brief Set invokable handler for given manipulator type and given data type
    /// @tparam M manipulator type (a distinct type allowing for multiple different handlers)
    /// @tparam C data/component type
    /// @param fn function callback
    template <class M, class C>
    static void set_handler(void (*fn)(M*, C&, uint gid, handler_data*), handler_data* data = nullptr)
    {
        manipulator<M>*& mm = handlers().component<manipulator<M>>();
        if (!mm)
            mm = new manipulator<M>;

        int cid = get_container_id<C>();

        using handler_t = typename handler<M, C>::handler_t;
        base_handler<M>*& h = mm->component_handlers.get_or_addc(cid);
        if (!h)
            h = new handler<M, C>(std::forward<handler_t>(fn), data);
    }

    /// @brief Invoke handler of given manipulator on given data type of given entity
    /// @tparam M manipulator type (a distinct type allowing for multiple different handlers)
    /// @tparam C data/component type
    /// @param m manipulator context
    /// @param gid global entity id
    template <class M, class C>
    static void invoke_component_handler(M* m, uint gid)
    {
        C* c = get<C>(gid);
        if (!c)
            return;

        manipulator<M>*& mm = handlers().component<manipulator<M>>();
        if (!mm)
            return;

        int cid = get_container_id<C>();

        base_handler<M>* h = mm->component_handlers.get_safe(cid, nullptr);
        if (!h)
            return;

        h->invoke(m, *c, gid);
    }

    /// @brief Invoke handlers of given manipulator for all data existing for given entity
    /// @tparam M manipulator type (a distinct type allowing for multiple different handlers)
    /// @param m manipulator context
    /// @param gid entity id
    /// @param fn optional callback to invoke before invoking a handler, identifying the data type processed
    template <class M>
    static void invoke_all_component_handlers(M* m, uint gid, void (*default_fn)(const coid::token& type) = 0)
    {
        manipulator<M>* mm = handlers().component<manipulator<M>>();
        if (!mm)
            return;

        uint n = data_containers().size();
        uint handler_count = mm->component_handlers.size();

        for (uint i = 0; i < n; ++i)
        {
            void* c = get_data(gid, i);
            if (!c)
                continue;

            base_handler<M>* h = i >= handler_count ? nullptr : mm->component_handlers[i];
            if (h) {
                h->invoke(m, c, gid);
            }
            else {
                if (default_fn) {
                    auto td = tsq().type(i);
                    default_fn(td->type_name);
                }
            }
        }
    }

    /// @brief Invoke given function for every component type, create component if fn returns true
    static void iterate_all_components(uint gid, bool (*fn)(const coid::token& type, bool entity_has_component) = 0)
    {
        if (fn == nullptr) return;

        uint n = data_containers().size();

        for (uint i = 0; i < n; ++i)
        {
            container* c = data_containers()[i];
            if (!c)
                continue;

            void* d = get_data(gid, i);
            bool has_component = d != nullptr;

            auto td = tsq().type(i);
            if (fn(td->type_name, has_component))
                c->create_default(gid);
        }
    }

private:

    template <class C>
    static container* get_container()
    {
        static uint cid = tsq().type_id<C>();
        dynarray32<container*>& co = data_containers();
        return cid < co.size() ? co[cid] : nullptr;
    }

    template <class Co>
    static Co* typed_container(uint cid)
    {
        dynarray32<container*>& co = data_containers();
        return cid < co.size() ? &static_cast<Co*>(co[cid]) : nullptr;
    }

    /// @brief get existing container or create a new (def.hashed) one
    /// @tparam C component type
    /// @param cid container id
    template <class C, class DefaultContainer = cshash<C>>
    static container& get_or_create_container()
    {
        static uint cid = tsq().type_id<C>();
        dynarray32<container*>& co = data_containers();
        if constexpr (DefaultContainer::specific_type != container::type::hash) {
            //mismatch between previously created container type
            DASSERT(cid >= co.size() || !co[cid] || co[cid]->storage_type == DefaultContainer::specific_type);
        }

        return  cid < co.size() && co[cid] != nullptr
            ? *co[cid]
            : *(co.get_or_addc(cid) = new DefaultContainer(cid));
    }

protected:

    struct containers
    {
        dynarray32<container*> _dc;
        coid::context_holder<OwnT> _hs;

        static containers& get() {
            LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(containers) _cs = new containers;
            return *_cs;
        }
    };

    static coid::context_holder<OwnT>& handlers() {
        return containers::get()._hs;
    }

    static dynarray32<container*>& data_containers() {
        return containers::get()._dc;
    }
};

COID_NAMESPACE_END

////////////////////////////////////////////////////////////////////////////////
class entman : public coid::data_manager<entman>
{
public:

    /// @brief current frame, determines the lifetime of references to components
    inline static uint frame = 0;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for managed data interfaces
/// @tparam T data interface type
template <class T>
class coref
{
public:

    coref() = default;
    coref(const coref&) = default;
    coref(coref&&) = default;

    coref(nullptr_t) {}

    /// @brief constructor from host data type
    explicit coref(T* host)
    {
        _entity_id = entman::get_versionid<T>(host);
        _cached_object = host;
        _ready_frame = entman::frame;
        _cid = entman::get_container_id<T>();
    }

    /// @brief Construct interface object (T) from host object
    /// @tparam HostType
    /// @param host host object pointer
    template <typename HostType>
    static coref<T> from_host(HostType* host)
    {
        coref<T> x;
        x._entity_id = entman::get_versionid<HostType>(host);
        x._cached_object = nullptr; //to be obtained in the first call, ensuring initialization
        x._ready_frame = -1;
        x._cid = entman::get_container_id<HostType>();
        return x;
    }

    /// @return component reference valid for current frame, nullptr if object no longer exists
    T* ready()
    {
        //obtain a valid reference once per frame
        uint gframe = entman::frame;
        if (_cached_object && _ready_frame == gframe)
            return _cached_object;

        T::get_data_ifc_descriptor();

        _cached_object = static_cast<T*>(entman::get_by_container_id(_entity_id, _cid));
        _ready_frame = gframe;
        return _cached_object;
    }

    /// @return true if this doesn't refer to any entity
    bool is_empty() const { return !_entity_id.is_valid(); }

    /// @return true if this refers to an entity
    /// @note only checks if this has been set up with a valid entity, but not if the entity is still valid - call ready() for that
    bool is_set() const { return _entity_id.is_valid(); }

    /// @return true if this refers to an entity
    /// @note only checks if this has been set up with a valid entity, but not if the entity is still valid - call ready() for that
    explicit operator bool() const { return _entity_id.is_valid(); }

    coref<T>& operator = (const coref<T>&) = default;

    T* operator -> () {
        T* p = ready();
        if (!p) throw coid::exception() << "dead object";
        return p;
    }

private:

    coid::versionid _entity_id;         //< id of the connected entity
    T* _cached_object = 0;              //< cached connected object
    uint _ready_frame = 0;              //< frame when the connected object was valid
    int _cid = -1;                      //< container id (from sequencer)
};
