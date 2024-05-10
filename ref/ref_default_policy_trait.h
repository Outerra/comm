#pragma once

#include "ref_policy_simple.h"

namespace coid
{

template<typename T>
struct default_ref_policy_trait
{
	using policy = ref_policy_simple<T>;
};

}; // end of namespace coid

/// @brief Macro used to override default ref policy trait for specified type
/// @param t - type
/// @param p - policy 
#define SET_DEFAULT_REF_POLICY_TRAIT(t, p) \
template<> struct coid::default_ref_policy_trait<t> { using policy = p<t>;};