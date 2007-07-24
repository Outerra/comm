

#ifdef _WIN32

#include "str.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "crtdefs.h"

COID_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
uints charstr::append_wchar_buf_ansi( const wchar_t* src, uints nchars )
{
    uints n = nchars==UMAX
        ? wcslen((const wchar_t*)src)
        : nchars;

    return WideCharToMultiByte( 0, 0, src, (int)n, get_append_buf(n), (int)n, 0, 0 );
}

///////////////////////////////////////////////////////////////////////////////
bool token::codepage_to_wstring_append( uint cp, std::wstring& dst ) const
{
    uints i = dst.length();
    dst.resize(i+len());

    wchar_t* data = const_cast<wchar_t*>(dst.data());
    uint n = MultiByteToWideChar( cp, 0, ptr(), len(), data+i, len() );

    dst.resize(i+n);
    return n>0;
}

COID_NAMESPACE_END


#endif //_WIN32
