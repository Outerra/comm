
#include "stdstream.h"

#ifdef SYSTYPE_WIN
#include <windows.h>

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
void stdoutstream::win_debug_out( const char* str )
{
    OutputDebugString(str);
}

COID_NAMESPACE_END

#endif
