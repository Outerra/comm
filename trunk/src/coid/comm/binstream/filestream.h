/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __COID_COMM_FILESTREAM__HEADER_FILE__
#define __COID_COMM_FILESTREAM__HEADER_FILE__

#include "../namespace.h"

#include <sys/stat.h>
#include "binstream.h"
#include "../str.h"

#include <fcntl.h>

#ifdef SYSTYPE_MSVC
# include <io.h>
# include <share.h>
#else
# include <unistd.h>
# define  O_RAW   0
# define _write     write
# define _read      read
# define _close     close
# define _fileno    fileno
#endif

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
class fileiostream : public binstream
{
    int   _handle;
    int   _rop;
    int64 _rpos, _wpos;

    void setpos( int64 pos )
    {
#ifdef SYSTYPE_MSVC
        RASSERT( -1 != _lseeki64( _handle, pos, SEEK_SET ) );
#else
        RASSERT( pos == lseek64( _handle, pos, SEEK_SET ) );
#endif
    }

    int64 getpos() const
    {
#ifdef SYSTYPE_MSVC
        return _telli64( _handle );
#else
        return lseek64( _handle, 0, SEEK_CUR );
#endif
    }

public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return 0;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        if(_rop)
        {
            _rpos = getpos();
            setpos( _wpos );
            _rop = 0;
        }

        len -= ::_write( _handle, p, len );
        return 0;
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        if(!_rop)
        {
            _wpos = getpos();
            setpos( _rpos );
            _rop = 1;
        }

        len -= ::_read( _handle, p, len );
        if( len > 0 )
            return ersNO_MORE "required more data than available";
        return 0;
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) { return ersUNAVAILABLE; }

    virtual bool is_open() const        { return _handle != -1; }
    virtual void flush()                { }
    virtual void acknowledge( bool eat=false )  { }

    virtual void reset()
    {
        //implement
    }

    virtual opcd open( const token& arg )
    {
        charstr name;
        token m = arg;
        token n = m.cut_left( '?', 1 );
        name = n;
        int flg=0;
        int rw=0,sh=0;
        
        while( !m.is_empty() )
        {
            if( m[0] == 'r' )       rw |= 1;
            else if( m[0] == 'w' )  rw |= 2;
            else if( m[0] == 'l' )  sh |= 1;
            else if( m[0] == 'e' )  flg |= O_EXCL;
            else if( m[0] == 'c' )  flg |= O_CREAT;
            else if( m[0] == 't' )  flg |= O_TRUNC;
            else if( m[0] == 'a' )  flg |= O_APPEND;
            //else if( m[0] == 'n' )  flg |= O_NONBLOCK;
            else if( m[0] != ' ' )
                throw ersINVALID_PARAMS;

            ++m;
        }

#ifdef SYSTYPE_WIN32
        int af;

        if( rw == 3 )       flg |= O_RDWR,      af = S_IREAD | S_IWRITE;
        else if( rw == 2 )  flg |= O_WRONLY,    af = S_IWRITE;
        else                flg |= O_RDONLY,    af = S_IREAD;

        if( sh == 1 )       sh = _SH_DENYWR;
        else                sh = _SH_DENYNO;

        flg |= O_BINARY;

# ifdef SYSTYPE_MSVC8plus
        return ::_sopen_s( &_handle, name.ptr(), flg, sh, af ) ? ersIO_ERROR : opcd(0);
# else
        _handle = ::_sopen( name.ptr(), flg, sh, af );
        return _handle != -1  ?  opcd(0)  :  ersIO_ERROR;
# endif
#else
        if( rw == 3 )       flg |= O_RDWR;
        else if( rw == 2 )  flg |= O_WRONLY;
        else                flg |= O_RDONLY;

        flg |= O_RAW;

        _handle = ::open( name.ptr(), flg, 0644 );
        if( _handle != -1  &&  sh )
        {
            int e = ::lockf( _handle, F_TLOCK, 0 );
            if( e != 0 )  close();
        }
        return _handle != -1  ?  opcd(0)  :  ersIO_ERROR;
#endif
    }

    virtual opcd close( bool linger=false )
    {
        if( _handle != -1 )
            ::_close(_handle);
        _handle = -1;
        return 0;
    }

    virtual opcd seek( int type, int64 pos )
    {
        if( type & fSEEK_CURRENT )
            pos += (type & fSEEK_READ) ? _rpos : _wpos;

        flush();
#ifdef SYSTYPE_MSVC
        opcd e = -1 != _lseeki64( _handle, pos, SEEK_SET )
            ?  opcd(0)
            :  ersFAILED;
#else
        opcd e = pos == lseek64( _handle, pos, SEEK_SET )
            ?  opcd(0)
            :  ersFAILED;
#endif
        if(!e)
        {
            if( type & fSEEK_READ )  _rpos = pos;
            if( type & fSEEK_WRITE ) _wpos = pos;
        }
        return e;
    }


    fileiostream() { _handle = -1; _wpos = _rpos = 0; _rop = 1; }
    explicit fileiostream( const token& s )
    {
        _handle = -1;
        open(s);
    }

    ~fileiostream() { close(); }
};


////////////////////////////////////////////////////////////////////////////////
class bofstream : public binstream
{
    FILE* _str;
public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return in0out1 ? 0 : fATTR_NO_INPUT_FUNCTION;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        len -= ::fwrite( p, 1, len, _str );
        return 0;
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        return ersNO_MORE;
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) { return ersUNAVAILABLE; }

    virtual bool is_open() const                { return _str != 0; }
    virtual void flush()                        { ::fflush (_str); }
    virtual void acknowledge( bool eat=false )  { }

    virtual void reset()
    {
        //implement
    }

    virtual opcd open( const token& arg )
    {
        charstr name;
        charstr mode;
        token m = arg;
        token n = m.cut_left( '?', 1 );
        name = n;
        mode = m;
        if( m.is_empty() )
            mode = "wb";
#ifdef SYSTYPE_MSVC8plus
        return fopen_s( &_str, name.ptr(), mode.ptr() ) ? ersIO_ERROR : opcd(0);
#else
        _str = fopen( name.ptr(), mode.ptr() );
        return _str != 0  ?  opcd(0)  :  ersIO_ERROR;
#endif
    }

    virtual opcd close( bool linger=false )
    {
        if(_str)
            fclose(_str);
        _str = 0;
        return 0;
    }

    virtual opcd seek( int type, int64 pos )
    {
        if( type & fSEEK_WRITE )
        {
#ifdef SYSTYPE_MSVC
            return -1 != ::_lseeki64( _fileno(_str), pos, (type & fSEEK_CURRENT) ? SEEK_CUR : SEEK_SET )
                ?  opcd(0)
                :  ersFAILED;
#else
            return 0 == ::fseeko64( _str, pos, (type & fSEEK_CURRENT) ? SEEK_CUR : SEEK_SET )
                ?  opcd(0)
                :  ersFAILED;
#endif
        }
        return ersINVALID_PARAMS;
    }


    bofstream() { _str = 0; }
    explicit bofstream( const token& s )
    {
        open(s);
    }

    ~bofstream() { close(); }
};

////////////////////////////////////////////////////////////////////////////////
class bifstream : public binstream
{
    FILE* _str;
public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return in0out1 ? fATTR_NO_OUTPUT_FUNCTION : 0;
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        len -= ::fread( p, 1, len, _str );
        if( len > 0 )
            return ersNO_MORE "required more data than available";
        return 0;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        return ersUNAVAILABLE "input file stream doesn't support writting";
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) { return ersUNAVAILABLE; }

    virtual bool is_open() const                { return _str != 0; }
    virtual void flush()                        { }
    virtual void acknowledge (bool eat=false)   { }

    virtual void reset()                        { }

    off_t size() const
	{
		struct stat s;
        if( 0 == ::fstat( _fileno(_str), &s ) )
            return s.st_size;
		return 0;
	}
    
    
    virtual opcd open( const token& arg )
    {
        close();

        charstr name;
        charstr mode;
        token m = arg;
        token n = m.cut_left( '?', 1 );
        name = n;
        mode = m;
        if( m.is_empty() )
            mode = "rb";
#ifdef SYSTYPE_MSVC8plus
        return ::fopen_s( &_str, name.ptr(), mode.ptr() ) ? ersIO_ERROR : opcd(0);
#else
        _str = ::fopen( name.ptr(), mode.ptr() );
        return _str != 0  ?  opcd(0)  :  ersIO_ERROR;
#endif
    }

    virtual opcd close( bool linger=false )
    {
        if(_str)
            fclose(_str);
        _str = 0;
        return 0;
    }

    virtual opcd seek( int type, int64 pos )
    {
        if( type & fSEEK_READ )
        {
#ifdef SYSTYPE_MSVC
            return -1 != ::_lseeki64( _fileno(_str), pos, (type & fSEEK_CURRENT) ? SEEK_CUR : SEEK_SET )
                ?  opcd(0)
                :  ersFAILED;
#else
            return 0 == ::fseeko64( _str, pos, (type & fSEEK_CURRENT) ? SEEK_CUR : SEEK_SET )
                ?  opcd(0)
                :  ersFAILED;
#endif
        }
        return ersINVALID_PARAMS;
    }


    bifstream() { _str = 0; }
    explicit bifstream( const token& s )
    {
        _str = 0;
        open(s);
    }

    ~bifstream() { close(); }
};

COID_NAMESPACE_END

#endif //__COID_COMM_FILESTREAM__HEADER_FILE__
