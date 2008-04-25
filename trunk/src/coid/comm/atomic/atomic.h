#ifndef __COMM_ATOMIC_H__
#define __COMM_ATOMIC_H__

#include "../commtypes.h"
#include "../commassert.h"

#if defined(WIN32)
	#include <intrin.h>

	#pragma intrinsic(_InterlockedIncrement)
	#pragma intrinsic(_InterlockedDecrement)
	#pragma intrinsic(_InterlockedExchangeAdd)
	#pragma intrinsic(_InterlockedCompareExchange)
	#pragma intrinsic(_InterlockedCompareExchange64)

	#if defined(SYSTYPE_64)
		#pragma intrinsic(_InterlockedCompareExchange128)
	#endif
#endif

namespace atomic {
	inline int32 inc(volatile int32 * ptr)
	{
		DASSERT(sizeof(int32) == 4 && sizeof(long) == 4);
#if defined(__GNUC__)
		return __sync_add_and_fetch(ptr, static_cast<int32>(1));
#elif defined(WIN32)
		return _InterlockedIncrement(reinterpret_cast<volatile long*>(ptr));
#endif
	}

	inline int32 dec(volatile int32 * ptr)
	{
		DASSERT(sizeof(int32) == 4 && sizeof(long) == 4);
#if defined(__GNUC__)
		return __sync_sub_and_fetch(ptr, static_cast<int32>(1));
#elif defined(WIN32)
		return _InterlockedDecrement(reinterpret_cast<volatile long*>(ptr));
#endif
	}

	inline int32 add(volatile int32 * ptr, const int32 val)
	{
		DASSERT(sizeof(int32) == 4 && sizeof(long) == 4);
#if defined(__GNUC__)
		return __sync_fetch_and_add(ptr, val);
#elif defined(WIN32)
		return _InterlockedExchangeAdd(
			reinterpret_cast<volatile long*>(ptr), val);
#endif
	}

	inline int32 cas(
		volatile int32 * ptr, const int32 val, const int32 cmp) 
	{
		DASSERT(sizeof(int32) == 4 && sizeof(long) == 4);
#if defined(__GNUC__)
		return __sync_val_compare_and_swap(ptr, cmp, val);
#elif defined(WIN32)
		return _InterlockedCompareExchange(
			reinterpret_cast<volatile long*>(ptr), val, cmp);
#endif
	}

	inline bool b_cas(
		volatile int32 * ptr, const int32 val, const int32 cmp) 
	{
		DASSERT(sizeof(int32) == 4 && sizeof(long) == 4);
#if defined(__GNUC__)
		return __sync_bool_compare_and_swap(ptr, cmp, val);
#elif defined(WIN32)
		return _InterlockedCompareExchange(
			reinterpret_cast<volatile long*>(ptr), val, cmp) == cmp;
#endif
	}

	inline int64 cas(
		volatile int64 * ptr, const int64 val, const int64 cmp) 
	{
		DASSERT(sizeof(int64) == 8);
#if defined(__GNUC__)
		return __sync_val_compare_and_swap(ptr, cmp, val);
#elif defined(WIN32)
		return _InterlockedCompareExchange64(
			reinterpret_cast<volatile int64*>(ptr), val, cmp);
#endif
	}

	inline bool b_cas(
		volatile int64 * ptr, const int64 val, const int64 cmp) 
	{
		DASSERT(sizeof(int64) == 8);
#if defined(__GNUC__)
		return __sync_bool_compare_and_swap(ptr, cmp, val);
#elif defined(WIN32)
		return _InterlockedCompareExchange64(
			reinterpret_cast<volatile int64*>(ptr), val, cmp) == cmp;
#endif
	}

#ifdef SYSTYPE_64
/*	inline bool b_cas(
		int64 * ptr, const int64 valh, const int64 vall, int64 * cmp) 
	{
		DASSERT(sizeof(int64) == 8);
#if defined(__GNUC__)
		return __sync_bool_compare_and_swap(ptr, cmp, val);
#elif defined(WIN32)
		return _InterlockedCompareExchange128(
			reinterpret_cast<int64 volatile*>(ptr), 
			valh, vall, 
			cmp) == cmp;
#endif
	}*/
#endif

	inline void * cas_ptr(void * volatile * pdst, void * pval, void * pcmp) 
	{
#if defined(__GNUC__)
		return reinterpret_cast<void*>(__sync_val_compare_and_swap(
			reinterpret_cast<volatile ints*>(pdst),
			reinterpret_cast<ints>(pcmp), 
			reinterpret_cast<ints>(pval)));
#elif defined(WIN32)
		return reinterpret_cast<void*>(_InterlockedCompareExchange(
			reinterpret_cast<volatile long *>(pdst),
			reinterpret_cast<long>(pval),
			reinterpret_cast<long>(pcmp)));
#endif
	}

	inline bool b_cas_ptr(void * volatile * pdst, void * pval, void * pcmp) 
	{
#if defined(__GNUC__)
		return __sync_bool_compare_and_swap(
			reinterpret_cast<volatile ints*>(pdst),
			reinterpret_cast<ints>(pcmp), 
			reinterpret_cast<ints>(pval));
#elif defined(WIN32)
		return _InterlockedCompareExchange(
			reinterpret_cast<volatile long *>(pdst),
			reinterpret_cast<long>(pval),
			reinterpret_cast<long>(pcmp)) == reinterpret_cast<long>(pcmp);
#endif
	}

} // end of namespace atomic

#endif // __COMM_ATOMIC_H__
