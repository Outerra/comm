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
    struct container {
        size_t element_size;
        enum class type {
            dynarray,
            slothash,
        } storage_type;

        container(uint size, enum class type t) : element_size(size), storage_type(t)
        {}
        virtual ~container() = default;

        /// @brief Fetch element from container, used only manipulator access
        virtual void* element(uint eid) = 0;
    };

    template <class T> struct storage : T { uint eid; };

    template <class T>
    struct cshash : container {
        struct extractor {
            using value_type = storage<T>;
            uint operator()(const storage<T>& s) const { return s.eid; }
        };

        cshash() : container(sizeof(storage<T>), container::type::slothash)
        {}
        virtual void* element(uint eid) override {
            return (T*)hash.find_value(eid);
        }

        slothash<storage<T>, uint, slotalloc_mode::versioning | slotalloc_mode::linear, extractor> hash;
    };

    template <class T>
    struct carray : container {
        carray() : container(sizeof(T), container::type::dynarray)
        {}
        virtual void* element(uint eid) override {
            return eid < data.size() ? &data[eid] : nullptr;
        }

        dynarray<T> data;
    };

    // manipulators for the data

    template <class M>
    struct base_handler {
        virtual ~base_handler() {}
        virtual void invoke(M*, void*) = 0;
    };

    template <class M, class C>
    struct handler : base_handler<M>
    {
        using handler_t = void (*)(M*, C&);

        virtual void invoke(M* m, void* c) override {
            return fn(m, *(C*)c);
        }

        handler(handler_t&& fn) {
            this->fn = std::move(fn);
        }

        handler_t fn;
    };

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
        type_sequencer& sq = tsq();
        int id = sq.id<C>();

        if (id < 0) {
            //negative ids are used to reference primary (linear) arrays instead of unordered map
            id = -1 - id;

            using coarray = carray<C>;
            coarray& co = data_container<coarray>(id);

            return eid < co.data.size()
                ? &co.data[eid]
                : nullptr;
        }

        using cohash = cshash<C>;
        cohash& co = data_container<cohash>(id);

        return (C*)co.hash.find_value(eid);
    }

    static void* get_data(uint eid, uint c)
    {
        container* co = data_containers().get_safe(c, nullptr);
        if (!co)
            return 0;

        return co->element(eid);
    }

    /// @brief Insert data of given type under the entity id
    /// @tparam C data type
    /// @param eid entity id
    /// @param v data value
    /// @return pointer to the inserted data
    template <class C>
    static C* push(uint eid, C&& v)
    {
        type_sequencer& sq = tsq();
        int id = sq.id<C>();

        //negative ids are used to reference primary arrays instead of unordered map
        if (id < 0) {
            id = -1 - id;

            using coarray = carray<C>;
            coarray& co = data_container<coarray>(id);

            C& sv = co.data.get_or_add(eid);
            sv = std::move(v);
            return &sv;
        }

        using cohash = cshash<C>;
        cohash& co = data_container<cohash>(id);

        bool isnew;
        storage<C>* sv = co.hash.find_or_insert_value_slot(eid, &isnew);
        *static_cast<C*>(sv) = std::move(v);

        sv->eid = eid;
        return sv;
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
    template <class C>
    static dynarray<C>& set_primary_container()
    {
        type_sequencer& sq = tsq();
        int& id = sq.assign<C>();

        if (id >= 0)
            id = -1 - id;
        int pid = -1 - id;

        using coarray = carray<C>;
        return data_container<coarray>(pid).data;
    }

    /// @brief Retrieve container for primary data
    /// @tparam C
    /// @return linear array of given data type
    template <class C>
    static dynarray<C>& get_primary_container()
    {
        type_sequencer& sq = tsq();
        const int* xid = sq.existing_id<C>();

        //primary containers must have been pre-loaded via set_primary_container<C>()
        if (xid == nullptr || *xid >= 0)
            throw exception();

        int pid = -1 - *xid;
        using coarray = dynarray<C>;
        return data_container<coarray>(pid);
    }

    /// @brief Set invokable handler for given manipulator type and given data type
    /// @tparam M manipulator type (a distinct type allowing for multiple different handlers)
    /// @tparam C data/component type
    /// @param fn function callback
    template <class M, class C>
    static void set_handler(void (*fn)(M*, C&))
    {
        manipulator<M>*& mm = handlers().component<manipulator<M>>();
        if (!mm)
            mm = new manipulator<M>;

        int cid = data_order<C>();

        using handler_t = typename handler<M, C>::handler_t;
        base_handler<M>*& h = mm->component_handlers.get_or_addc(cid);
        if (!h)
            h = new handler<M, C>(std::forward<handler_t>(fn));
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

        h->fn(m, *c);
    }

    /// @brief Invoke handlers of given manipulator for all data existing for given entity
    /// @tparam M manipulator type (a distinct type allowing for multiple different handlers)
    /// @param m manipulator context
    /// @param eid entity id
    /// @param fn optional callback to invoke before invoking a handler, identifying the data type processed
    template <class M>
    static void invoke_all_component_handlers(M* m, uint eid, bool (*fn)(const coid::token& type) = 0)
    {
        manipulator<M>* mm = handlers().component<manipulator<M>>();
        if (!mm)
            return;

        uint n = data_containers().size();

        for (uint i = 0; i < n; ++i) {
            base_handler<M>* h = mm->component_handlers[i];
            if (!h)
                continue;

            void* c = get_data(eid, i);
            if (c) {
                if (fn) {
                    auto td = tsq().type(i);
                    if (!fn(td->type_name))
                        continue;
                }

                h->invoke(m, c);
            }
        }
    }

protected:

    static type_sequencer& tsq() {
        LOCAL_PROCWIDE_SINGLETON_DEF(type_sequencer) _tsq = new type_sequencer;
        return *_tsq;
    }

private:

    template <class T>
    static T& data_container(uint id)
    {
        container*& vctx = data_containers().get_or_addc(id);
        T*& cont = reinterpret_cast<T*&>(vctx);

        if (!cont)
            cont = new T;
        return *cont;
    }

protected:

    static coid::context_holder<OwnT>& handlers() {
        LOCAL_SINGLETON_DEF(coid::context_holder<OwnT>) _hs;
        return *_hs;
    }

    static dynarray32<container*>& data_containers() {
        LOCAL_SINGLETON_DEF(dynarray32<container*>) _dc;
        return *_dc;
    }
};

COID_NAMESPACE_END
