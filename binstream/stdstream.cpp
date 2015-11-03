
#include "stdstream.h"

#ifdef SYSTYPE_WIN
#include <windows.h>
#endif

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
void stdoutstream::debug_out( const char* str )
{
#ifdef SYSTYPE_WIN
    OutputDebugString(str);
#endif
}

COID_NAMESPACE_END
