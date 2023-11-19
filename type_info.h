#pragma once

#include "token.h"
#include "commtypes.h"

#pragma once

namespace coid
{
struct type_info
{
    token name;
    uint hash;

    template<typename Type>
    static constexpr const type_info& get()
    {
        static constexpr type_info result = {coid::token::type_name<Type>(), coid::token::type_name<Type>().hash()};
        return result;
    }
};


}; // end of namespace coid
