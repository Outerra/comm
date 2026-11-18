#pragma once

#include "token.h"
#include "commtypes.h"

namespace coid
{

/// @brief A type's information made at compile time - an object per type and module (get): its readable name and its
/// identity, the hash of get's decorated name
struct type_info
{
    token name;                                 //< the readable name (token::type_name - __FUNCSIG__'s): types of one name share it (anonymous namespaces' types of two units)
    uint64 hash;                                //< the type's identity - the 64-bit FNV-1a hash of get's decorated name (MSVC: __FUNCDNAME__ - the type's mangled name in it, the same in every module, an anonymous namespace tagged by its unit; other compilers: __PRETTY_FUNCTION__ - types of one name share it), computed at compile time; memtrack's key - one type's allocations together across modules, types of one name apart

    template<typename Type>
    static constexpr const type_info& get()
    {
#ifdef SYSTYPE_MSVC
        static constexpr type_info result = {coid::token::type_name<Type>(), token_literal(__FUNCDNAME__).hash64()};
#else
        static constexpr type_info result = {coid::token::type_name<Type>(), token_literal(__PRETTY_FUNCTION__).hash64()};
#endif
        return result;
    }
};


}; // end of namespace coid
