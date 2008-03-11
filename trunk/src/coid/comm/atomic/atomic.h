#ifndef __COMM_ATOMIC_H__
#define __COMM_ATOMIC_H__

#if defined(__GNUC__)
	#if defined(powerpc) || defined(__ppc__)
		#include "atomic_ppc.h"
	#else
		#include "atomic_gcc.h"
	#endif
#elif defined(WIN32)
	#include "atomic_win32.h"
#endif

#endif // __COMM_ATOMIC_H__
