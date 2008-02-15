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
# define _dup       dup
#endif

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
class filestream : public binstream
{
public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return 0;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        if(_op>0 )
            upd_rpos();

        DASSERT( _handle != -1 );

        uint k = ::_write( _handle, p, (uint)len );
        _wpos += k;
        len -= k;
        return 0;
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        if(_op<0)
            upd_wpos();

        DASSERT( _handle != -1 );

        uint k = ::_read( _handle, p, (uint)len );
        _rpos += k;
        len -= k;
        if( len > 0 )
            return ersNO_MORE "required more data than available";
        return 0;
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) { return ersUNAVAILABLE; }

    virtual opcd peek_read( uint timeout ) {
        if(timeout)  return ersINVALID_PARAMS;
        return (uint64)_rpos < size()  ?  opcd(0) : ersNO_MORE;
    }

    virtual opcd peek_write( uint timeout ) {
        return 0;
    }


    virtual bool is_open() const        { return _handle != -1; }
    virtual void flush()                { }
    virtual void acknowledge( bool eat=false )  { }

    virtual void reset_read()
    {
        _rpos = 0;
        if(_op>0)
            setpos(_rpos);
    }

    virtual void reset_write()
    {
        _wpos = 0;
        if(_op<0)
            setpos(_wpos);
    }

    //@{ Get and set current reading and writing position
    uint64 get_read_pos() const         { return _op>0  ?  getpos()  :  _rpos; }
    uint64 get_write_pos() const        { return _op<0  ?  getpos()  :  _wpos; }

    bool set_read_pos( uint64 pos )     {
        if(_op<0)
            upd_wpos();

        return setpos(pos);
    }

    bool set_write_pos( uint64 pos )     {
        if(_op>0)
            upd_rpos();

        return setpos(pos);
    }
    //@}

    ///Open file
    //@param name file name
    //@param attr open attributes
    /// r - open for reading
    /// w - open for writing
    /// l - lock file
    /// e - fail if file already exists
    /// c - create
    /// t,- - truncate
    /// a,+ - append
    virtual opcd open( const token& name, token attr = token::empty() )
    {
        charstr tmp = name;
        return open( tmp.c_str(), attr );
    }

    ///Open file
    //@param name file name
    //@param attr open attributes
    /// r - open for reading
    /// w - open for writing
    /// l - lock file
    /// e - fail if file already exists
    /// c - create
    /// t,- - truncate
    /// a,+ - append
    opcd open( const char* name, token attr = token::empty() )
    {
        int flg=0;
        int rw=0,sh=0;
        
        while( !attr.is_empty() )
        {
            if( attr[0] == 'r' )       rw |= 1;
            else if( attr[0] == 'w' )  rw |= 2;
            else if( attr[0] == 'l' )  sh |= 1;
            else if( attr[0] == 'e' )  flg |= O_EXCL;
            else if( attr[0] == 'c' )  flg |= O_CREAT;
            else if( attr[0] == 't' || attr[0] == '-' )  flg |= O_TRUNC;
            else if( attr[0] == 'a' || attr[0] == '+' )  flg |= O_APPEND;
            //else if( m[0] == 'n' )  flg |= O_NONBLOCK;
            else if( attr[0] != ' ' )
                return ersINVALID_PARAMS;

            ++attr;
        }

        _rpos = _wpos = 0;

#ifdef SYSTYPE_WIN32
        int af;

        if( rw == 3 )       flg |= O_RDWR,      af = S_IREAD | S_IWRITE;
        else if( rw == 2 )  flg |= O_WRONLY,    af = S_IWRITE;
        else                flg |= O_RDONLY,    af = S_IREAD;

        if( sh == 1 )       sh = _SH_DENYWR;
        else                sh = _SH_DENYNO;

        flg |= O_BINARY;

# ifdef SYSTYPE_MSVC8plus
        return ::_sopen_s( &_handle, name, flg, sh, af ) ? ersIO_ERROR : opcd(0);
# else
        _handle = ::_sopen( name, flg, sh, af );
        return _handle != -1  ?  opcd(0)  :  ersIO_ERROR;
# endif
#else
        if( rw == 3 )       flg |= O_RDWR;
        else if( rw == 2 )  flg |= O_WRONLY;
        else                flg |= O_RDONLY;

        flg |= O_RAW;

		/*Handle hfile = CreateFile( 
			name,
			GENERIC_READ, // desired access
			0,                            // share mode (none)
			0,                         // security attributes
			OPEN_EXISTING,                // fail if it doesn't exist
			FILE_FLAG_NO_BUFFERING,    // flags & attributes
			0);                       // file template


		if( hfile == INVALID_HANDLE_VALUE ) {
            int e = ::lockf( _handle, F_TLOCK, 0 );
            if( e != 0 )  close();
		}

		_handle = _open_osfhandle((intptr_t)hfile, 0);*/

		_handle = ::open( name, flg, 0644 );

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
        if( _handle != -1 ) {
            ::_close(_handle);
            _handle = -1;
        }

        _rpos = _wpos = 0;

        return 0;
    }

    ///Duplicate filestream
    opcd dup( filestream& dst ) const
    {
        if( _handle == -1 )
            return ersIMPROPER_STATE;

        dst.close();
        dst._handle = ::_dup(_handle);

        return 0;
    }

    virtual opcd seek( int type, int64 pos )
    {
        if( type & fSEEK_CURRENT )
            pos += (type & fSEEK_READ) ? _rpos : _wpos;

        int op = (type&fSEEK_READ) ? 1 : -1;

        if( op == _op  &&  !setpos(pos) )
            return ersFAILED;

        if(op<0)
            _wpos = pos;
        else
            _rpos = pos;

        return 0;
    }

    uint64 size() const
	{
		struct stat s;
        if( 0 == ::fstat( _handle, &s ) )
            return s.st_size;
		return 0;
	}

    filestream() { _handle = -1; _wpos = _rpos = 0; _op = 1; }

    explicit filestream( const token& s )
    {
        _handle = -1;
        _rpos = _wpos = 0;
        open(s);
    }

    filestream( const token& s, token attr )
    {
        _handle = -1;
        _rpos = _wpos = 0;
        open(s,attr);
    }

    filestream( const char* s, token attr )
    {
        _handle = -1;
        _rpos = _wpos = 0;
        open(s,attr);
    }

    ~filestream() { close(); }


private:

    int   _handle;
    int   _op;                          ///< >0 reading, <0 writing
    int64 _rpos, _wpos;

    bool setpos( int64 pos )
    {
#ifdef SYSTYPE_MSVC
        return -1 != _lseeki64( _handle, pos, SEEK_SET );
#else
        return pos == lseek64( _handle, pos, SEEK_SET );
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

    ///Update _wpos and switch to reading mode
    void upd_wpos() {
        _wpos = getpos();
        setpos(_rpos);
        _op = 1;
    }

    ///Update _rpos and switch to writing mode
    void upd_rpos() {
        _rpos = getpos();
        setpos(_wpos);
        _op = -1;
    }
};

//compatibility
typedef filestream fileiostream;


////////////////////////////////////////////////////////////////////////////////
class bofstream : public filestream
{
public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return in0out1 ? 0 : fATTR_NO_INPUT_FUNCTION;
    }

    virtual opcd open( const token& arg )
    {
        token name = arg;
        charstr flg = name.cut_right_back( '?', 1, true );
        if( flg.is_empty() )
            flg = "wct";
        else
            flg << char('w');
        return filestream::open( name, flg );
    }

    bofstream() { }
    explicit bofstream( const token& s )
    {
        open(s);
    }

    ~bofstream() { }
};

////////////////////////////////////////////////////////////////////////////////
class bifstream : public filestream
{
public:
    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return in0out1 ? 0 : fATTR_NO_OUTPUT_FUNCTION;
    }

    virtual opcd open( const token& arg )
    {
        token name = arg;
        charstr flg = name.cut_right_back( '?', 1, true );
        flg << char('r');
        return filestream::open( name, flg );
    }

    bifstream() { }
    explicit bifstream( const token& s )
    {
        open(s);
    }

    ~bifstream() { }
};

COID_NAMESPACE_END

#endif //__COID_COMM_FILESTREAM__HEADER_FILE__
