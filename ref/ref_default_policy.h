#pragma once

#include "ref_policy_shared.h"

namespace coid
{

template<typename T>
struct default_ref_policy
{
	using policy = ref_policy_shared<T>;
};

}; // end of namespace coid

/// @brief Macro used to override default ref policy for specified type
/// @param t - type
/// @param p - policy 
#define DEFAULT_REF_POLICY(t, p) \
template<> struct coid::default_ref_policy<t> { using policy = p<t>;};