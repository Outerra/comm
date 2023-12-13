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
 * Portions created by the Initial Developer are Copyright (C) 2021
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

    //@return reference to a component pointer
    //@param pid [optional] receives component id
    template <class T>
    T*& component(int* pid = 0)
    {
        type_sequencer& sq = tsq();
        int id = sq.id<T>();
        if (pid)
            *pid = id;
        void*& ctx = _components.get_or_addc(id);
        return reinterpret_cast<T*&>(ctx);
    }

    //@return component id (sequential by order of registration)
    template <class T>
    int component_id() {
        type_sequencer& sq = tsq();
        return sq.id<T>();
    }

    void* component_data(int cid) const {
        return cid < (int)_components.size() ? _components[cid] : nullptr;
    }

private:

    static type_sequencer& tsq() {
        LOCAL_PROCWIDE_SINGLETON_DEF(type_sequencer) _tsq = new type_sequencer;
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

    struct sequencer : type_sequencer
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
            if (clear_bit(id)) {
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

        versionid get_versionid(uint eid) const {
            DASSERT_RET(get_bit(eid), versionid());
            return versionid(eid, _entities[eid].version);
        }

        dynarray32<uints> _allocated;
        dynarray32<erecord> _entities;
        uint _count = 0;
    };

    static sequencer& tsq() {
        LOCAL_PROCWIDE_SINGLETON_DEF(sequencer) _tsq = new sequencer;
        return *_tsq;
    }



    struct container
    {
        size_t element_size;

        enum class type {
            array,
            hash,
        } storage_type;

        sequencer* seq = 0;

        container(uint size, enum class type t) : element_size(size), storage_type(t) {
            seq = &tsq();
        }
        virtual ~container() = default;

        /// @brief Fetch element from container
        virtual void* element(uint eid) = 0;

        /// @brief Create element
        virtual void* create_default(uint eid) = 0;
        virtual void* create_uninit(uint eid) = 0;

        virtual void remove(uint eid) = 0;

        /// @return pointer to a dynarray with linear storage
        virtual const void* linear_array_ptr() const = 0;

        /// @brief Reserve memory
        /// @param count number of elements to reserve memory for
        virtual void reserve(uint count) = 0;

        virtual versionid allocate() = 0;
    };

    /// @brief helper class to hold entity id for hashed data containers
    template <class T> struct storage : T {
        uint eid;
    };

    /// @brief slothash type container for sparse data
    /// @tparam T
    template <class T>
    struct cshash : container {
        struct extractor {
            using value_type = storage<T>;
            uint operator()(const storage<T>& s) const { return s.eid; }
        };

        cshash() : container(sizeof(storage<T>), container::type::hash)
        {}
        void* element(uint eid) override final { return (T*)hash.find_value(eid); }
        void* create_default(uint eid) override {
            return new (create_uninit(eid)) T;
        }
        void* create_uninit(uint eid) override {
            bool is_new;
            storage<T>* item = hash.find_or_insert_value_slot_uninit(eid, &is_new);
            if (!is_new)
                item->~storage<T>();
            item->eid = eid;
            return item;
        }
        void remove(uint eid) override final {
            hash.erase(eid);
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

        slothash<storage<T>, uint, slotalloc_mode::versioning | slotalloc_mode::linear | slotalloc_mode::atomic, extractor> hash;
    };

    /// @brief linear array container
    /// @tparam T
    template <class T>
    struct carray : container {
        carray() : container(sizeof(T), container::type::array)
        {}
        void* element(uint eid) override final { return eid < data.size() ? &data[eid] : nullptr; }
        void* create_default(uint eid) override {
            return new (create_uninit(eid)) T;
        }
        void* create_uninit(uint eid) override {
            return &data.get_or_add(eid);
        }
        void remove(uint eid) override final {
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

        virtual void invoke(M* m, void* c, uint eid) override {
            return fn(m, *(C*)c, eid, data);
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

    /// @brief Retrieve data of given type
    /// @tparam C data type
    /// @param eid entity id
    /// @return data pointer
    template <class C>
    static C* get(uint eid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();

        return c ? static_cast<C*>(c->element(eid)) : nullptr;
    }

    /// @brief Retrieve data of given type
    /// @tparam C data type
    /// @param eid entity id
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

    static void* get_data(uint eid, uint c)
    {
        container* co = data_containers().get_safe(c, nullptr);
        if (!co)
            return 0;

        return co->element(eid);
    }

    static versionid allocate()
    {
        static sequencer& seq = tsq();
        uint id;
        erecord* er = seq.add(&id);

        return versionid(id, er->version);
    }

    /// @brief Free entity
    static void free(versionid vid) {
        static sequencer& seq = tsq();
        if (seq.del(vid)) {
            for (container* c : data_containers()) {
                if (c && c->storage_type == container::type::hash)
                    c->erase(vid.id);
            }
        }
    }

    /// @brief Free entity
    static void free(uint eid) {
        static sequencer& seq = tsq();
        if (seq.del(eid)) {
            for (container* c : data_containers()) {
                if (c && c->storage_type == container::type::hash)
                    c->remove(eid);
            }
        }
    }

    /// @brief Run destructor on component
    /// @tparam OwnT
    template <class C>
    static void destruct(uint eid) {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c);
        C* co = static_cast<C*>(c->element(eid));
        if (co)
            co->~C();
    }

    static bool is_valid(uint eid) {
        static sequencer& seq = tsq();
        return seq.is_valid(eid);
    }

    static bool is_valid(versionid vid) {
        static sequencer& seq = tsq();
        return seq.is_valid(vid);
    }

    static versionid get_versionid(uint eid) {
        static sequencer& seq = tsq();
        return seq.get_versionid(eid);
    }

    /// @brief Insert data of given type under the entity id
    /// @tparam C data type
    /// @param eid entity id
    /// @param v data value to move from
    /// @return pointer to the inserted data
    template <class C>
    static C* push(uint eid, C&& v)
    {
        static container* c = get_or_create_container<C, cshash<C>>();
        DASSERT_RET(c, 0);

        C* d = static_cast<C*>(c->create_default(eid));
        *d = std::move(v);
        DASSERT(c->storage_type != container::type::hash || static_cast<storage<C>*>(d)->eid == eid);

        return d;
    }

    /// @brief Insert default constructed data
    /// @tparam C data type
    /// @param eid entity id
    /// @return pointer to the uninitialized data
    template <class C>
    static C* push_default(uint eid)
    {
        static container* c = get_or_create_container<C, cshash<C>>();
        DASSERT_RET(c, 0);

        return static_cast<C*>(c->create_default(eid));
    }

    /// @brief Insert uninitialized data, to be in-place constructed
    /// @tparam C data type
    /// @param eid entity id
    /// @return pointer to the uninitialized data
    template <class C, class...Ps>
    static C* emplace(uint eid, Ps&&... ps)
    {
        static container* c = get_or_create_container<C, cshash<C>>();
        DASSERT_RET(c, 0);

        return new (c->create_uninit(eid)) C(std::forward<Ps>(ps)...);
    }

    /// @brief Remove component from entity
    /// @tparam C data type
    /// @param eid entity id
    template <class C>
    static void remove(uint eid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c);

        c->remove(eid);
    }

    /// @brief Remove component from entity
    /// @tparam C data type
    /// @param eid entity id
    template <class C>
    static void remove(versionid vid)
    {
        static container* c = 0;
        if (!c) c = get_container<C>();
        DASSERT_RET(c);

        DASSERT_RET(c->seq->is_valid(vid));
        c->remove(vid.id);
    }

    /// @brief Enumerate through entities with given component
    /// @tparam C data type
    /// @tparam Fn
    /// @param fn
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

    /// @brief Get order id assigned to the type (also assigns on first use)
    /// @tparam C
    /// @return -1 if type was not registered
    template <class C>
    static int data_order()
    {
        int id = tsq().id<C>();
        return id < 0 ? -1 - id : id;
    }

    /// @brief Define a primary data type, that will be stored in a linear array indexed directly
    /// @tparam C data type
    /// @param reserve_count number of elements to reserve initial memory for
    /// @return container id
    /// @note container id 0 is created as a slotalloc to keep track of existing and deleted objects
    template <class C>
    static void preallocate_array_container(uint reserve_count = 1 << 23)
    {
        carray<C>* cont = get_or_create_container<C, carray<C>>();
        cont->reserve(reserve_count);
    }

    template <class C>
    static void preallocate_hash_container(uint reserve_count = 1 << 23)
    {
        cshash<C>* cont = get_or_create_container<C, cshash<C>>();
        cont->reserve(reserve_count);
    }

    /// @brief Retrieve container for primary data
    /// @tparam C
    /// @return linear array of given data type, or nullptr if given storage doesn't have linear data
    template <class C>
    static const dynarray32<C>& get_array()
    {
        static const dynarray32<C>* c = static_cast<const dynarray32<C>*>(get_or_create_container<C, carray<C>>()->linear_array_ptr());
        RASSERT(c != nullptr);

        return *c;
    }

    template <class C>
    static uint get_id_in_array(const C* p)
    {
        return uint(p - get_array<C>().ptr());
    }

    template <class C>
    static versionid get_versionid_in_array(const C* p)
    {
        return get_versionid(uint(p - get_array<C>().ptr()));
    }


    /// @brief Set invokable handler for given manipulator type and given data type
    /// @tparam M manipulator type (a distinct type allowing for multiple different handlers)
    /// @tparam C data/component type
    /// @param fn function callback
    template <class M, class C>
    static void set_handler(void (*fn)(M*, C&, uint eid, handler_data*), handler_data* data = nullptr)
    {
        manipulator<M>*& mm = handlers().component<manipulator<M>>();
        if (!mm)
            mm = new manipulator<M>;

        int cid = data_order<C>();

        using handler_t = typename handler<M, C>::handler_t;
        base_handler<M>*& h = mm->component_handlers.get_or_addc(cid);
        if (!h)
            h = new handler<M, C>(std::forward<handler_t>(fn), data);
    }

    /// @brief Invoke handler of given manipulator on given data type of given entity
    /// @tparam M manipulator type (a distinct type allowing for multiple different handlers)
    /// @tparam C data/component type
    /// @param m manipulator context
    /// @param eid entity id
    template <class M, class C>
    static void invoke_component_handler(M* m, uint eid)
    {
        C* c = get<C>(eid);
        if (!c)
            return;

        manipulator<M>*& mm = handlers().component<manipulator<M>>();
        if (!mm)
            return;

        int cid = data_order<C>();

        base_handler<M>* h = mm->component_handlers.get_safe(cid, nullptr);
        if (!h)
            return;

        h->invoke(m, *c, eid);
    }

    /// @brief Invoke handlers of given manipulator for all data existing for given entity
    /// @tparam M manipulator type (a distinct type allowing for multiple different handlers)
    /// @param m manipulator context
    /// @param eid entity id
    /// @param fn optional callback to invoke before invoking a handler, identifying the data type processed
    template <class M>
    static void invoke_all_component_handlers(M* m, uint eid, void (*default_fn)(const coid::token& type) = 0)
    {
        manipulator<M>* mm = handlers().component<manipulator<M>>();
        if (!mm)
            return;

        uint n = data_containers().size();
        uint handler_count = mm->component_handlers.size();

        for (uint i = 0; i < n; ++i)
        {
            void* c = get_data(eid, i);
            if (!c)
                continue;

            base_handler<M>* h = i >= handler_count ? nullptr : mm->component_handlers[i];
            if (h) {
                h->invoke(m, c, eid);
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
    static void iterate_all_components(uint eid, bool (*fn)(const coid::token& type, bool entity_has_component) = 0)
    {
        if (fn == nullptr) return;

        uint n = data_containers().size();

        for (uint i = 0; i < n; ++i)
        {
            container* c = data_containers()[i];
            if (!c)
                continue;

            void* d = get_data(eid, i);
            bool has_component = d != nullptr;

            auto td = tsq().type(i);
            if (fn(td->type_name, has_component))
                c->create_default(eid);
        }
    }

private:

    template <class C>
    static container* get_container()
    {
        static uint cid = tsq().id<C>();
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
    template <class C, class ContDefault = cshash<C>>
    static ContDefault* get_or_create_container()
    {
        static uint cid = tsq().id<C>();
        dynarray32<container*>& co = data_containers();
        return static_cast<ContDefault*>(
            cid < co.size() && co[cid] != nullptr ? co[cid] : (co.get_or_addc(cid) = new ContDefault()));
    }

protected:

    struct containers
    {
        dynarray32<container*> _dc;
        coid::context_holder<OwnT> _hs;

        static containers& get() {
            LOCAL_PROCWIDE_SINGLETON_DEF(containers) _cs = new containers;
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
