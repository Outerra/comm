#pragma once

#include "ref/ref_intrusive.h"

using policy_intrusive_base = coid::ref_policy_intrusive;

template<typename Type>
using iref = coid::ref_intrusive<Type>;