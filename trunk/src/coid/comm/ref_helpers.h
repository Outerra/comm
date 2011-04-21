#pragma once
#ifndef __COID_REF_HELPERS_H__
#define __COID_REF_HELPERS_H__

#include "str.h"

namespace coid {

// takeover

template<class T>
struct queue_helper_trait {
    static void take(T& dst,T& src) { dst=src; }
    static void release(T& item) {}
};

template<class K>
struct queue_helper_trait<ref<K>> {
    static void take(ref<K>& dst,ref<K>& src) { dst.takeover(src); }
    static void release(ref<K>& item) { item.release(); }
};

template<class K>
struct queue_helper_trait<iref<K>> {
    static void take(iref<K>& dst,iref<K>& src) { dst.takeover(src); }
    static void release(iref<K>& item) { item.release(); }
};

template<>
struct queue_helper_trait<coid::charstr> {
    static void take(coid::charstr &dst,coid::charstr &src) { dst.takeover(src); }
    static void release(coid::charstr& item) { item.reset(); }
};

template<class K>
struct queue_helper_trait<coid::dynarray<K>> {
    static void take(coid::dynarray<K>& dst,coid::dynarray<K>& src) { dst.takeover(src); }
    static void release(coid::dynarray<K>& item) { item.reset(); }
};

} // end of namespace coid

#endif // __COID_REF_HELPERS_H__