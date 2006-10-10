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
    if( _pattern.last_char() == '\\' )
        _pattern.trim_to_length(-1);

    if( 0 != stat( _pattern.ptr(), &_st ) )
        return ersFAILED;

    _pattern << '\\';
    _curpath = _pattern;
    _baselen = _curpath.len();
    _pattern << filter;

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
opcd directory::mkdir( const charstr& name, uint mode )
{
    if(!::_mkdir( name.ptr() ))  return 0;
    if( errno == EEXIST )  return 0;
    return ersFAILED;
}

////////////////////////////////////////////////////////////////////////////////
charstr& directory::get_cwd( charstr& buf )
{
    uints size = 64;

    while( !::_getcwd( buf.get_append_buf(size-1), size )  &&  size < 1024 )
    {
        size <<= 1;
        buf.reset();
    }
    buf.correct_size();
    if( buf.last_char() != '\\' )
        buf << "\\";
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
int directory::chdir( const charstr& name )
{
    return ::_chdir( name.ptr() );
}

////////////////////////////////////////////////////////////////////////////////
const struct stat* directory::next()
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

    _curpath.trim_to_length( _baselen );
    _curpath << dir.name;
    if( 0 == stat( _curpath.ptr(), &_st ) )
        return &_st;

    return next();
}


COID_NAMESPACE_END


#endif //SYSTYPE_MSVC
