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
#include "str.h"
#include "sync/mutex.h"
#include "type_info.h"

#include <atomic>
#ifndef SYSTYPE_MSVC
#include <typeinfo>
#endif

COID_NAMESPACE_BEGIN


/// @brief a registrar that assigns unique sequential id for any queried type
/// A type is identified by its 64-bit hash (get_type_hash_internal - coid::type_info's, of its decorated name), not by
/// its name: one type gets one id in every module (its mangled name - what the linker matches - is the same in every
/// module: a process-wide sequencer gives a dll module the id the main module assigned), types of one name text get ids
/// of their own (anonymous namespaces' types of two units - token::type_name gives both one text); two types of one
/// hash would share an id - a sequencer of n types has that chance of about n^2 / 2^65. An entry keeps a copy of its
/// type's name, so the unload of the module that queried a type first leaves its entry valid - a reloaded module's
/// types get their ids again; a module's type of another size than its entry's (a module built with another definition
/// of it) is asserted in Debug. The type's entry is cached in a static variable of a template specialization for the
/// owner and the type (id_holder - constant-initialized, one per module), so repeated queries are fast and take no
/// lock; a type's first query in a module takes the mutex and reads the cache again (another thread may have cached the
/// entry while it waited) - concurrent first queries of a type get one id. The cache is the OWNER's, not an instance's:
/// one instance per OWNER, or each type queried by one of them only (comm's data_manager - its data types and its
/// handlers' types); a cached entry outside this instance's reserved space (another instance's, a destroyed one's) is
/// asserted in Debug. Created by a module that outlives its other users (the main module asking for a process-wide one
/// first): its allocate_internal is its creator's code
template<typename OWNER>
class type_sequencer
{
public:
    /// @brief An id no type has - an entry's id until its type's first query assigns it
    static constexpr int invalid_type_id = -1;

    /// @brief A type's record - never moved (the reserved space): the queries' cache points to it; its name its own
    /// (copied at its type's first query)
    struct entry {
        token type_name;                        //< token::type_name's text, of name_text - for display (types of one name text have entries of their own)
        size_t size = 0;                        //< sizeof - the first query's
        int id = invalid_type_id;               //< its id - its index in the sequencer's types
        uint64 hash = 0;                        //< its identity - the type's hash (get_type_hash_internal), compared by allocate_internal
        charstr name_text;                      //< type_name's text - a copy of the querying module's
    };

private:

    /// @brief The entries' reserved space - an entry never moves (the queries' cache points to it)
    static constexpr uint max_types = 1 << 20;

    /// @brief Type T's entry - cached by its first query in this module: the OWNER's (its instances share it), each
    /// module's own; constant-initialized (constinit - no initializer runs, no initialization order)
    template <class T>
    struct id_holder {
        inline static constinit std::atomic<const entry*> cached_entry{nullptr};
    };

public:

    type_sequencer() : _mux(500, true)
    {
        _types.reserve_virtual(max_types);
        _types_begin = _types.ptr();
    }

    virtual ~type_sequencer() {}

    /// @brief Get or assign an id for type T
    /// @tparam T
    /// @return cached or assigned id
    /// @note IDs are generated sequentially, 0-based index
    template <class T>
    int get_type_id()
    {
        return get_entry_internal<T>(true)->id;
    }

    /// @brief Get an existing id that was assigned to the type
    /// @tparam T
    /// @return the id's address (in its entry - never moved), nullptr if the id wasn't assigned yet
    template <class T>
    const int* get_existing_id() {
        const entry* en = get_entry_internal<T>(false);
        return en ? &en->id : nullptr;
    }

    /// @brief Pre-allocate type, with a callback run for it under the mutex - at each call, also for a type assigned
    /// before (comm's ifcman fills an interface's descriptor at its id: a module registering again replaces it)
    /// @tparam T
    /// @param cbk optional callback executed under mutex lock - the type's id and entry; it may query the sequencer
    /// (the mutex is recursive)
    /// @return the id. Value of invalid_type_id is reserved for uninitialized state.
    template <class T>
    int assign(const function<void(int, const entry&)>& cbk)
    {
        constexpr token ti = token::type_name<T>();
        const uint64 type_hash = get_type_hash_internal<T>();
        comm_mutex_guard g(_mux);
        const entry* en = allocate_internal(type_hash, ti, sizeof(T), true);
        if (cbk)
            cbk(en->id, *en);
        id_holder<T>::cached_entry.store(en, std::memory_order_release);
        return en->id;
    }

    const dynarray32<entry>& get_types() const {
        return _types;
    }

    const entry* get_type(uint type_id) const {
        return type_id < _types.size() ? &_types[type_id] : nullptr;
    }

    /// @brief Get type T's name - its entry's (token::type_name's text, a copy of its own); a type without an entry
    /// gets one (its id assigned)
    template<class T>
    const coid::token& get_type_name()
    {
        return get_entry_internal<T>(true)->type_name;
    }

private:

    /// @brief Get type T's entry - cached; otherwise, under the mutex, the cache read again (another thread of this
    /// module may have cached it while this one waited), then found or added and cached; a cached entry not this
    /// instance's (is_own_entry_internal) asserted in Debug
    /// @param create whether to add the entry when the type has none (its id assigned)
    /// @return the entry, nullptr when the type has none and create is false
    template <class T>
    const entry* get_entry_internal(bool create)
    {
        const entry* en = id_holder<T>::cached_entry.load(std::memory_order_acquire);
        if (!en) {
            //needs allocating (or finding if this is a query from different module)
            constexpr token ti = token::type_name<T>();
            const uint64 type_hash = get_type_hash_internal<T>();
            comm_mutex_guard g(_mux);

            //another thread of this module may have cached it while this one waited for the mutex
            en = id_holder<T>::cached_entry.load(std::memory_order_acquire);
            if (!en) {
                en = allocate_internal(type_hash, ti, sizeof(T), create);
                if (en)
                    id_holder<T>::cached_entry.store(en, std::memory_order_release);
            }
        }

        DASSERTX(!en || is_own_entry_internal(en), "type_sequencer: the cached entry of " << token::type_name<T>() << " isn't this "
            "sequencer's - another sequencer of its OWNER cached it, or a destroyed one (the cache is the OWNER's)");
        return en;
    }

    /// @brief Whether an entry lies in this instance's reserved space - a cached entry of another instance of the
    /// OWNER, or of a destroyed one, doesn't (the cache is the OWNER's)
    bool is_own_entry_internal(const entry* en) const
    {
        return (uints)en - (uints)_types_begin < (uints)max_types * sizeof(entry);
    }

    /// @brief A type's hash - its identity: the same for the type in every module, another type's another (but at the
    /// chance of 64 bits): MSVC's coid::type_info::hash (of get's decorated name - an anonymous namespace's types
    /// tagged by their unit, no RTTI); other compilers' hash of typeid(T).name() - an internal linkage type's (its name
    /// starting with '*': compared by its std::type_info's address there, in its own module only) with that address
    /// hashed in
    template <class T>
    static uint64 get_type_hash_internal()
    {
#ifdef SYSTYPE_MSVC
        return coid::type_info::get<T>().hash;
#else
        const std::type_info& rtti = typeid(T);
        const token name = token::from_cstring(rtti.name());
        uint64 type_hash = name.hash64();
        if (name.first_char() == '*') {
            const uint64 address = (uint64)(uints)&rtti;
            type_hash = __coid_hash_string64(reinterpret_cast<const char*>(&address), sizeof(address), type_hash);
        }
        return type_hash;
#endif
    }

    //virtual here is for always calling the main module implementation and not (possibly) outdated dll one - the
    //sequencer's creator, which outlives its other users
    /// @brief Find a type's entry by its hash (get_type_hash_internal), add one when there's none and create is set
    /// (the next id, the type's name copied); called under the mutex
    /// @param size the querying module's sizeof - another than the entry's asserted in Debug (a module built with
    /// another definition of the type)
    /// @return the entry, nullptr when there's none and create is false
    virtual entry* allocate_internal(uint64 type_hash, const token& type_name, size_t size, bool create)
    {
        for (entry& en : _types) {
            if (en.hash == type_hash) {
                DASSERTX(en.size == size, "type_sequencer: " << type_name << " of " << (uint)size << " bytes here, of "
                    << (uint)en.size << " in its entry - a module built with another definition of it");
                return &en;
            }
        }
        if (!create)
            return nullptr;

        uint id = _types.size();
        entry* en = _types.add();
        en->hash = type_hash;
        en->name_text = type_name;
        en->type_name = en->name_text;
        en->size = size;
        en->id = id;
        return en;
    }

    dynarray32<entry> _types;
    comm_mutex _mux;
    const entry* _types_begin = nullptr;        //< _types' reserved space's start - set by the constructor only (_types.ptr() is written again as the space commits), read without the mutex by the queries' Debug check (is_own_entry_internal)
};

COID_NAMESPACE_END
