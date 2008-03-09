#ifndef __COMM_ATOMIC_WIN32_H__
#define __COMM_ATOMIC_WIN32_H__

#include "../commtypes.h"
#include "../assert.h"
#include <intrin.h>

#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedCompareExchange64)

#if (SYSTYPE_64)
	#pragma intrinsic(_InterlockedCompareExchange128)
#endif

namespace atomic {
	typedef coid::int32 aint32;
	typedef coid::int64 aint64;

	inline aint32 inc(volatile aint32 * ptr)
	{
		DASSERT(sizeof(aint32) == 4 && sizeof(long) == 4);
		return _InterlockedIncrement(reinterpret_cast<long volatile*>(ptr));
	}

	inline aint32 dec(volatile aint32 * ptr)
	{
		DASSERT(sizeof(aint32) == 4 && sizeof(long) == 4);
		return _InterlockedDecrement(reinterpret_cast<long volatile*>(ptr));
	}

	inline aint32 cas(
		aint32 * ptr, const aint32 val, const aint32 cmp) 
	{
		DASSERT(sizeof(aint32) == 4 && sizeof(long) == 4);

		return _InterlockedCompareExchange(
			reinterpret_cast<long volatile*>(ptr), val, cmp);
	}

	inline bool b_cas(
		aint32 * ptr, const aint32 val, const aint32 cmp) 
	{
		DASSERT(sizeof(aint32) == 4 && sizeof(long) == 4);

		return _InterlockedCompareExchange(
			reinterpret_cast<long volatile*>(ptr), val, cmp) == cmp;
	}

	inline aint64 cas(
		aint64 * ptr, const aint64 val, const aint64 cmp) 
	{
		DASSERT(sizeof(aint64) == 8 && sizeof(__int64) == 8);

		return _InterlockedCompareExchange64(
			reinterpret_cast<__int64 volatile*>(ptr), val, cmp);
	}

	inline bool b_cas(
		aint64 * ptr, const aint64 val, const aint64 cmp) 
	{
		DASSERT(sizeof(aint64) == 8 && sizeof(__int64) == 8);

		return _InterlockedCompareExchange64(
			reinterpret_cast<__int64 volatile*>(ptr), val, cmp) == cmp;
	}

#ifdef SYSTYPE_64
	inline bool b_cas(
		aint64 * ptr, const aint64 valh, const aint64 vall, aint64 * cmp) 
	{
		DASSERT(sizeof(aint64) == 8 && sizeof(__int64) == 8);

		return _InterlockedCompareExchange128(
			reinterpret_cast<__int64 volatile*>(ptr), 
			valh, vall, 
			cmp) == cmp;
	}
#endif

	inline void * cas_ptr(void * * pdst, void * pval, void * pcmp) 
	{
		return reinterpret_cast<void*>(_InterlockedCompareExchange(
			reinterpret_cast<long volatile *>(pdst),
			reinterpret_cast<long>(pval),
			reinterpret_cast<long>(pcmp)));
	}

	inline bool b_cas_ptr(void * * pdst, void * pval, void * pcmp) 
	{
		return _InterlockedCompareExchange(
			reinterpret_cast<long volatile *>(pdst),
			reinterpret_cast<long>(pval),
			reinterpret_cast<long>(pcmp)) == reinterpret_cast<long>(pcmp);
	}

} // end of namespace atomic

#endif // __COMM_ATOMIC_WIN32_H__
