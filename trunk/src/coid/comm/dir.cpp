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
#include "binstream/filestream.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////

bool directory::stat( const char* name, xstat* dst )
{
    return 0 == ::_stat64( name, dst );
}

bool directory::stat( const charstr& name, xstat* dst )
{
    return 0 == ::_stat64( name.c_str(), dst );
}

bool directory::is_valid( const char* dir )
{
    xstat st;
    return _stat64(dir, &st)==0;
}

bool directory::is_valid_directory( const char* arg )
{
    xstat st;
    return _stat64(arg, &st)==0  &&  is_directory(st.st_mode);
}

bool directory::is_valid_file( const char* arg )
{
    xstat st;
    return _stat64(arg, &st)==0  &&  is_regular(st.st_mode);
}

uint64 directory::file_size( const charstr& file )
{
    xstat st;
    if(_stat64(file.c_str(), &st)==0  &&  is_regular(st.st_mode))
        return st.st_size;

    return 0;
}

uint64 directory::file_size( const char* file )
{
    xstat st;
    if(_stat64(file, &st)==0  &&  is_regular(st.st_mode))
        return st.st_size;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::append_path( charstr& dst, token path )
{
#ifdef SYSTYPE_WIN
    if( path.nth_char(1) == ':' )
#else
    if( path.first_char() == '/' )
#endif
        dst = path;
    else
    {
        token tdst = dst;
        if( directory::is_separator( tdst.last_char() ) )
            tdst--;

        while( path.begins_with("..") )
        {
            path.shift_start(2);
            char c = path.first_char();

            if( c!=0 && !is_separator(c) )
                return false;       //bad path, .. not followed by separator

            if( tdst.is_empty() )
                return false;       //too many .. in path

            tdst.cut_right_back("\\/");

            if( c == 0 ) {
                dst.resize( tdst.len() );
                return true;
            }

            ++path;
        }

        dst.resize( tdst.len() );

        if( !is_separator( dst.last_char() ) )
            dst << separator();

        dst << path;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_file_from( const token& src, const token& name )
{
    _curpath.resize(_baselen);

    if( name.is_empty() )
    {
        //extract name from the source path
        token srct = src;
        token srcfn = srct.cut_right_back( separator() );
        _curpath << srcfn;
    }
    else
        _curpath << name;
    
    return copy_file( src, _curpath );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_file_to( const token& dst, const token& name )
{
    _curpath.resize(_baselen);

    if( name.is_empty() )
    {
        //extract name from the destination path
        token dstt = dst;
        token srcfn = dstt.cut_right_back( separator() );
        _curpath << srcfn;
    }
    else
        _curpath << name;
    
    return copy_file( _curpath, dst );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_current_file_to( const token& dst )
{
    return copy_file( _curpath, dst );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_file( const token& src, const token& dst )
{
    if( src == dst )
        return 0;

    fileiostream fsrc, fdst;

    opcd e = fsrc.open(src);
    if(e)
        return e;

    e = fdst.open(dst, "wct");
    if(e)
        return e;

    char buf[8192];
    for(;;)
    {
        uints len = 8192;
        opcd re = fsrc.read_raw( buf, len );
        if( len < 8192 )
        {
            uints den = 8192-len;
            fdst.write_raw( buf, den );
            if( den > 0 )
                return ersIO_ERROR "write operation failed";
        }
        else if( re == ersNO_MORE )
            break;
        else
            return re;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_file_from( const token& src, const token& name )
{
    _curpath.resize(_baselen);

    if( name.is_empty() )
    {
        //extract name from the source path
        token srct = src;
        token srcfn = srct.cut_right_back( separator() );
        _curpath << srcfn;
    }
    else
        _curpath << name;
    
    return move_file( src, _curpath );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_file_to( const token& dst, const token& name )
{
    _curpath.resize(_baselen);

    if( name.is_empty() )
    {
        //extract name from the destination path
        token dstt = dst;
        token srcfn = dstt.cut_right_back( separator() );
        _curpath << srcfn;
    }
    else
        _curpath << name;
    
    return move_file( _curpath, dst );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_current_file_to( const token& dst )
{
    return move_file( _curpath, dst );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_file( const char* src, const char* dst )
{
    if( 0 == rename(src,dst) )
        return 0;
    return ersIO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::delete_file( const char* src )
{
#ifdef SYSTYPE_MSVC
    return 0 == _unlink(src)  ?  opcd(0) : ersIO_ERROR;
#else
    return 0 == unlink(src) ? opcd(0) : ersIO_ERROR;
#endif
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::delete_files( token path_and_pattern )
{
    directory dir;
    opcd e = dir.open(path_and_pattern);
    if(e) return e;

    while(dir.next()) {
        opcd le = delete_file(dir.get_last_full_path());
        if(le) e = le;
    }

    return e;
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::mkdir_tree( token name, uint mode )
{
    while( name.last_char() == '/' || name.last_char() == '\\' )
        name.shift_end(-1);

    charstr path = name;
    char* pc = (char*)path.c_str();

    for( uint i=0; i<name.len(); ++i )
    {
        if( name[i] == '/'  ||  name[i] == '\\' )
        {
            char c = pc[i];
            pc[i] = 0;

            opcd e = mkdir(path, mode);
            if(e)  return e;

            pc[i] = c;
        }
    }

    return mkdir(path, mode);
}


COID_NAMESPACE_END
