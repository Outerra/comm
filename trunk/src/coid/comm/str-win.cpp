
#ifdef _WIN32

#define TOKEN_SUPPORT_WSTRING
#include "token.h"
#include "str.h"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

COID_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
uint charstr::append_wchar_buf_ansi( const wchar_t* src, uints nchars )
{
    uints n = nchars==UMAXS
        ? wcslen((const wchar_t*)src)
        : nchars;

    return WideCharToMultiByte( CP_ACP, 0, src, (int)n, get_append_buf(n), (int)n, 0, 0 );
}

///////////////////////////////////////////////////////////////////////////////
bool token::codepage_to_wstring_append( uint cp, std::wstring& dst ) const
{
    uints i = dst.length();
    dst.resize(i+len());

    wchar_t* data = const_cast<wchar_t*>(dst.data());
    uint n = MultiByteToWideChar( cp, 0, ptr(), (uint)len(), data+i, (uint)len() );

    dst.resize(i+n);
    return n>0;
}

///////////////////////////////////////////////////////////////////////////////
bool token::utf8_to_wchar_buf( wchar_t* dst, uints maxlen ) const
{
    if(maxlen) --maxlen;
    const char* p = ptr();
    bool succ = true;

    uints i=0;
    for(; i<maxlen; ++i)
    {
        if( (uchar)*p <= 0x7f ) {
            dst[i] = *p++;
        }
        else
        {
            uints ne = get_utf8_seq_expected_bytes(p);
            if(p+ne > _pte) {
                //error in input
                succ = false;
                break;
            }

            dst[i] = (wchar_t)read_utf8_seq(p);
            p += ne;
        }
    }

    dst[i] = 0;
    return succ;
}

COID_NAMESPACE_END


#endif //_WIN32

////////////////////////////////////////////////////////////////////////////////

#include "str.h"
#include "pthreadx.h"
#include "atomic/stack_base.h"

///////////////////////////////////////////////////////////////////////////////

#include <sstream>

namespace std {

ostream& operator << (ostream& ost, const coid::charstr& str)
{
    ost.write(str.c_str(), str.len());
    return ost;
}

ostream& operator << (ostream& ost, const coid::token& str)
{
    ost.write(str.ptr(), str.len());
    return ost;
}

} //namespace std

///////////////////////////////////////////////////////////////////////////////

COID_NAMESPACE_BEGIN


typedef atomic::stack_base<charstr*> pool_t;

////////////////////////////////////////////////////////////////////////////////
static atomic::stack_base<charstr*>& zeroterm_pool()
{
    static thread_key _TK;

    pool_t* p = (pool_t*)_TK.get();
    if(p)
        return *p;
        
    p = new pool_t;
    _TK.set(p);

    return *p;
}

////////////////////////////////////////////////////////////////////////////////
zstring::~zstring()
{
    if(_buf) {
        zeroterm_pool().push(_buf);
        _buf = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////
zstring::zstring(const zstring& s)
{
    _buf = 0;
    if(s._buf) {
        _zptr = s._buf->ptr();
        _zend = s._buf->ptre();
    }
    else {
        _zptr = s._zptr;
        _zend = s._zend;
    }
}

////////////////////////////////////////////////////////////////////////////////
zstring::zstring(const char* sz)
    : _zptr(sz), _zend(0), _buf(0)
{}

////////////////////////////////////////////////////////////////////////////////
zstring::zstring(const token& tok)
    : _zptr(0), _zend(0)
{
    pool_t& pool = zeroterm_pool();
    if(!pool.pop(_buf))
        _buf = new charstr;
    else
        _buf->reset();

    *_buf = tok;
}

////////////////////////////////////////////////////////////////////////////////
zstring::zstring(const charstr& str)
    : _zptr(str.ptr()), _zend(str.ptre()), _buf(0)
{}

////////////////////////////////////////////////////////////////////////////////
const char* zstring::c_str() const
{
    return _buf ? _buf->c_str() : _zptr;
}

////////////////////////////////////////////////////////////////////////////////
token zstring::get_token() const
{
    if(_buf)
        return token(*_buf);
    else if(_zptr && !_zend)
        _zend = _zptr + ::strlen(_zptr);

    return token(_zptr, _zend);
}

////////////////////////////////////////////////////////////////////////////////
///Get modifiable string
charstr& zstring::get_str()
{
    if(!_buf) {
        pool_t& pool = zeroterm_pool();
        if(!pool.pop(_buf))
            _buf = new charstr;
        else
            _buf->reset();

        if(_zend)
            _buf->set_from_range(_zptr, _zend);
        else
            _buf->set(_zptr);
    }

    return *_buf;
}


COID_NAMESPACE_END
