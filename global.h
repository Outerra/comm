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
    template <class T> struct storage : T { uint eid; };

    template <class T>
    struct shash {
        struct extractor {
            using value_type = storage<T>;
            uint operator()(const storage<T>& s) const { return s.eid; }
        };

        virtual ~shash() {}
        slothash<storage<T>, uint, slotalloc_mode::versioning | slotalloc_mode::linear, extractor> hash;
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

        //negative ids are used to reference primary (linear) arrays instead of unordered map
        if (id < 0) {
            id = -1 - id;

            using coarray = dynarray<C>;
            coarray& co = data_container<coarray>(id);

            return eid < co.size()
                ? &co[eid]
                : nullptr;
        }

        using cohash = shash<C>;
        cohash& co = data_container<cohash>(id);

        return (C*)co.hash.find_value(eid);
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

            using coarray = dynarray<C>;
            coarray& co = data_container<coarray>(id);

            C& sv = co.get_or_add(eid);
            sv = std::move(v);
            return &sv;
        }

        using cohash = shash<C>;
        cohash& co = data_container<cohash>(id);

        bool isnew;
        storage<C>* sv = co.hash.find_or_insert_value_slot(eid, &isnew);
        *static_cast<C*>(sv) = std::move(v);

        sv->eid = eid;
        return sv;
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

        using coarray = dynarray<C>;
        return data_container<coarray>(pid);
    }

    /// @brief Retrieve container for primary data
    /// @tparam C
    /// @return linear array of given data type
    template <class C>
    static dynarray<C>& get_primary_container()
    {
        type_sequencer& sq = tsq();
        const int* xid = sq.existing_id<C>();

        //primary containers must have been pre-loaded via primary<C>()
        if (xid == nullptr || *xid >= 0)
            throw exception();

        int pid = -1 - *xid;
        using coarray = dynarray<C>;
        return data_container<coarray>(pid);
    }

private:

    static type_sequencer& tsq() {
        LOCAL_PROCWIDE_SINGLETON_DEF(type_sequencer) _tsq = new type_sequencer;
        return *_tsq;
    }

    template <class T>
    static T& data_container(uint id)
    {
        void*& vctx = _data_containers.get_or_addc(id);
        T*& cont = reinterpret_cast<T*&>(vctx);

        if (!cont)
            cont = new T;
        return *cont;
    }

    inline static dynarray32<void*> _data_containers;
};

COID_NAMESPACE_END
