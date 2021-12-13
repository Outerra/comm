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
#include "token.h"
#include "dynarray.h"
#include "sync/mutex.h"

COID_NAMESPACE_BEGIN


/// @brief a registrar that assigns unique sequential id for any queried type
/// The assigned ID is cached in a static variable of a template specialization for the type, so repeated
/// queries are fast. First time initialization and first time request from a
class type_sequencer
{
    template <class T>
    struct id_holder {
        inline static int slot = -1;
    };

public:

    type_sequencer() : _mux(500, false)
    {}

    virtual ~type_sequencer() {}

    /// @brief Get unique 0-based id for this type
    /// @tparam T
    /// @return cached or assigned id
    template <class T>
    int id()
    {
        int& rid = id_holder<T>::slot;
        if (rid >= 0)
            return rid;

        //needs allocating (or finding if this is a query from different module)
        constexpr token ti = token::type_name<T>();
        return rid = allocate(ti);
    }

private:

    //virtual here is for always calling the main module implementation and not (possibly) outdated dll one
    virtual int allocate(const token& type_name)
    {
        comm_mutex_guard g(_mux);
        int i = 0;
        for (const token& tok : _types) {
            if (tok == type_name)
                return i;
            ++i;
        }
        *_types.add() = type_name;
        return i;
    }

    dynarray<token> _types;
    comm_mutex _mux;
};


/// @brief class for fast access of heterogenous data as components
/// @note keeps T* in array, given T is always in the same slot across all instances of context_holder
/// @tparam OwnT owner class (or any type) used to store shared data
template <class OwnT>
class context_holder
{
public:

    //@return reference to a component pointer
    template <class T>
    T*& component() {
        type_sequencer& sq = tsq();
        int id = sq.id<T>();
        void*& ctx = _components.get_or_addc(id);
        return reinterpret_cast<T*&>(ctx);
    }

private:

    type_sequencer& tsq() {
        LOCAL_PROCWIDE_SINGLETON_DEF(type_sequencer) _tsq = new type_sequencer;
        return *_tsq;
    }

    dynarray32<void*> _components;
};


COID_NAMESPACE_END
