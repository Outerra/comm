

#ifdef _WIN32

#include "str.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "crtdefs.h"

COID_NAMESPACE_BEGIN

uints charstr::append_wchar_buf_ansi( const ushort* src, uints nchars )
{
    uints n = nchars==UMAX
        ? wcslen((const wchar_t*)src)
        : nchars;

    return WideCharToMultiByte( 0, 0, (const wchar_t*)src, n, get_buf(n), n, 0, 0 );
}

COID_NAMESPACE_END


#endif //_WIN32
