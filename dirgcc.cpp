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



#ifndef SYSTYPE_MSVC

#include <fnmatch.h>
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
    _dir = 0;
}

////////////////////////////////////////////////////////////////////////////////
char directory::separator()             { return '/'; }
const char* directory::separator_str()  { return "/"; }

////////////////////////////////////////////////////////////////////////////////
opcd directory::open( const token& path, const token& filter )
{
    close();

    _curpath = path;
    if( _curpath.last_char() == '/' )
        _curpath.trim_to_length(-1);

    _dir = opendir( _curpath.ptr() );
	if(!_dir)
		return ersFAILED;

    _curpath << '/';
    _baselen = _curpath.len();
    
    stat( _curpath.ptr(), &_st );

    _pattern = filter;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
void directory::close()
{
    if(_dir)
        closedir(_dir);
    _dir = 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_entry_open() const
{
    return _dir != 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_entry_directory() const
{
    return S_ISDIR(_st.st_mode);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_entry_regular() const
{
    return S_ISREG(_st.st_mode);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_directory( ushort mode )
{
    return S_ISDIR(mode);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_regular( ushort mode )
{
    return S_ISREG(mode);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::mkdir( const char* name, mode_t mode )
{
    if(!::mkdir(name,mode))  return 0;
    if( errno == EEXIST )  return 0;
    return ersFAILED;
}

////////////////////////////////////////////////////////////////////////////////
charstr& directory::get_cwd( charstr& buf )
{
    uints size = 64;

    while( !getcwd( buf.get_append_buf(size-1), size )  &&  size < 1024 )
    {
        size <<= 1;
        buf.reset();
    }
    buf.correct_size();
    if( buf.last_char() != '/' )
        buf << "/";
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
int directory::chdir( const char* name )
{
    return ::chdir(name);
}

////////////////////////////////////////////////////////////////////////////////
const struct stat* directory::next()
{
    dirent* dire = readdir(_dir);
    if(!dire)
        return 0;

    if( 0 == fnmatch( _pattern.ptr(), dire->d_name, 0 ) )
    {
        _curpath.trim_to_length( _baselen );
        _curpath << dire->d_name;
        if( 0 == stat( _curpath.ptr(), &_st ) )
            return &_st;
    }

    return next();
}


COID_NAMESPACE_END


#endif //SYSTYPE_MSVC
