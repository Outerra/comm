
#include "stdstream.h"

extern "C" {

__declspec(dllimport)
void
__stdcall
OutputDebugStringA(
    const char* lpOutputString
);

}

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
void stdoutstream::debug_out( const char* str )
{
#ifdef SYSTYPE_WIN
    OutputDebugStringA(str);
#endif
}

COID_NAMESPACE_END
