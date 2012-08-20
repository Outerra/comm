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

#include "dir.h"


#ifdef SYSTYPE_MSVC

#include <direct.h>
#include <io.h>
#include <errno.h>

extern "C" {
ulong __stdcall GetModuleFileNameA(void*, char*, ulong);
void* __stdcall GetCurrentProcess();
long __stdcall OpenProcessToken(void*, long, void**);
int __stdcall GetUserProfileDirectoryA(void*, char*, ulong*);
long __stdcall CloseHandle(void*);
ulong __stdcall GetTempPathA(ulong, char*);

#pragma comment(lib, "userenv.lib")
}

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
directory::~directory()
{
    close();
}

////////////////////////////////////////////////////////////////////////////////
directory::directory()
{
    _handle = -1;
}

////////////////////////////////////////////////////////////////////////////////
char directory::separator()             { return '\\'; }
const char* directory::separator_str()  { return "\\"; }

////////////////////////////////////////////////////////////////////////////////
opcd directory::open( const token& path, const token& filter )
{
    close();

    _pattern = path;
    if(_pattern.len() > 1  &&  _pattern.len() <= 3  &&  _pattern.nth_char(1) == ':')
        treat_trailing_separator(_pattern, true);
    else if( _pattern.last_char() == '\\' || _pattern.last_char() == '/' )
        _pattern.resize(-1);

    if(_stat64(_pattern.ptr(), &_st) != 0)
        return ersFAILED;

    if(_pattern.last_char() != '\\')
        _pattern << '\\';
    _curpath = _pattern;
    _baselen = _curpath.len();
    _pattern << (filter ? filter : token("*.*"));

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
void directory::close()
{
    if( _handle != -1 )
        _findclose(_handle);
    _handle = -1;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_entry_open() const
{
    return _handle != -1;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_entry_directory() const
{
    return (_S_IFDIR & _st.st_mode) != 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_entry_regular() const
{
    return (_S_IFREG & _st.st_mode) != 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_directory( ushort mode )
{
    return (_S_IFDIR & mode) != 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_regular( ushort mode )
{
    return (_S_IFREG & mode) != 0;
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::mkdir( const char* name, uint mode )
{
#ifdef SYSTYPE_WIN
    if(name[1] == ':' && name[2] == 0)
        return 0;
#endif
    if(!_mkdir(name))  return 0;
    if( errno == EEXIST )  return 0;
    return ersFAILED;
}

////////////////////////////////////////////////////////////////////////////////
charstr& directory::get_cwd( charstr& buf )
{
    uint size = 64;

    while( size < 1024  &&  !::_getcwd( buf.get_buf(size-1), size) )
    {
        size <<= 1;
        buf.reset();
    }
    buf.correct_size();
    
    return treat_trailing_separator(buf, true);
}

////////////////////////////////////////////////////////////////////////////////
charstr& directory::get_program_path( charstr& buf )
{
    uint size = 64, asize;

    while( size < 1024  &&  size == (asize = GetModuleFileNameA(0, buf.get_buf(size-1), size)) )
    {
        size <<= 1;
        buf.reset();
    }

    buf.resize(asize);
    compact_path(buf);

    return buf;
}

////////////////////////////////////////////////////////////////////////////////
charstr& directory::get_ped( charstr& buf )
{
    get_program_path(buf);

    token t = buf;
    t.cut_right_back('\\', token::cut_trait_keep_sep_with_source());

    return buf.resize( t.len() );
}

////////////////////////////////////////////////////////////////////////////////
charstr& directory::get_tmp( charstr& buf )
{
    GetTempPathA(244, buf.get_buf(244));
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
int directory::chdir( const char* name )
{
    return ::_chdir(name);
}

////////////////////////////////////////////////////////////////////////////////
const directory::xstat* directory::next()
{
    _finddata_t dir;
    
    if( _handle == -1 )
    {
        _handle = _findfirst( _pattern.ptr(), &dir );
        if( _handle == -1 )
            return 0;
    }
    else
    {
        if( 0 != _findnext( _handle, &dir ) )
            return 0;
    }

    _curpath.resize( _baselen );
    _curpath << dir.name;
    if(_stat64(_curpath.ptr(), &_st) == 0)
        return &_st;

    return next();
}

////////////////////////////////////////////////////////////////////////////////
charstr& directory::get_home_dir( charstr& path )
{
    ulong psize = 260;
    path.get_buf(psize);
   
    void* hToken = 0;
    if(!OpenProcessToken( GetCurrentProcess(), 0x0008/*TOKEN_QUERY*/, &hToken ))
        return path;
   
    if(!GetUserProfileDirectoryA(hToken, (char*)path.ptr(), &psize))
        return path;
   
    CloseHandle(hToken);
    
    path.correct_size();
    treat_trailing_separator(path, true);
    return path;
}


COID_NAMESPACE_END


#endif //SYSTYPE_MSVC
