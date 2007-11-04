#ifndef __COMM_ATOMIC_WIN32_H__
#define __COMM_ATOMIC_WIN32_H__

namespace atomic {

template <class T>
inline char cas(T volatile * pAddr, const T pValue, const T pComperand) 
{
	register char c;
	__asm {
		push	ebx
		push	esi
		mov		esi, pAddr
		mov		eax, pComperand
		mov		ebx, pValue
		lock cmpxchg dword ptr [esi], ebx
		sete	c
		pop		esi
		pop		ebx
	}
	return c;
}

template<class T, class Z>
inline char cas(
	void volatile * pAddr, 
	const T valueL, 
	const Z uiValueH,
	const T comperandL, 
	const Z uiComperandH)
{
	register char c;
	__asm {
		push	ebx
		push	ecx
		push	edx
		push	esi
		mov		esi, pAddr
		mov		eax, comperandL
		mov		edx, uiComperandH
		mov		ebx, valueL
		mov		ecx, uiValueH
		lock cmpxchg8b qword ptr [esi]
		sete	c
		pop		esi
		pop		edx
		pop		ecx
		pop		ebx
	}
	return c;
}

inline void inc(volatile int * pValue)
{
	__asm {
		push	esi
		mov		esi, pValue
		lock inc dword ptr [esi]
		pop		esi
	}
}

inline void dec(volatile int * pValue)
{
	__asm {
		push	esi
		mov		esi, pValue
		lock dec dword ptr [esi]
		pop		esi
	}
}

} // end of namespace atomic

#endif // __COMM_ATOMIC_WIN32_H__
