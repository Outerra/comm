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
#include "str.h"
#include "pthreadx.h"

#include <sys/utime.h>

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
bool directory::stat( const zstring& name, xstat* dst )
{
    return 0 == ::_stat64( name.c_str(), dst );
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_valid( const zstring& dir )
{
    xstat st;
    return _stat64(dir.c_str(), &st)==0;
}

////////////////////////////////////////////////////////////////////////////////
static bool _is_valid_directory( const char* arg )
{
    directory::xstat st;
    return _stat64(arg, &st)==0  &&  directory::is_directory(st.st_mode);
}

bool directory::is_valid_directory( const zstring& arg )
{
    token tok = arg.get_token();

    bool dosdrive = tok.len()==2 && tok[1]==':';
    bool lastsep = tok.last_char() == '\\' || tok.last_char() == '/';

    zstring marg = arg;
    if(!dosdrive && lastsep) {
        charstr& tmp = marg.get_str();
        tmp.resize(-1);
    }
    else if(dosdrive && !lastsep) {
        charstr& tmp = marg.get_str();
        tmp << separator();
    }

    return _is_valid_directory(marg.c_str());
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_valid_file( const zstring& arg )
{
    if(!arg.c_str())
        return false;

    xstat st;
    return _stat64(arg.c_str(), &st)==0  &&  is_regular(st.st_mode);
}

////////////////////////////////////////////////////////////////////////////////
uint64 directory::file_size( const zstring& file )
{
    if(!file.c_str())
        return 0;

    xstat st;
    if(_stat64(file.c_str(), &st)==0  &&  is_regular(st.st_mode))
        return st.st_size;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_absolute_path( const token& path )
{
#ifdef SYSTYPE_WIN
    return path.nth_char(1) == ':' || path.begins_with("\\\\");
#else
    return path.first_char() == '/';
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool directory::append_path( charstr& dst, token path )
{

    if(is_absolute_path(path))
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

            tdst.cut_right_back(DIR_SEPARATORS);

            if( c == 0 ) {
                dst.resize( tdst.len() );
                return true;
            }

            ++path;
        }

        dst.resize( tdst.len() );

        if( dst && !is_separator( dst.last_char() ) )
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
opcd directory::copy_file( const zstring& src, const zstring& dst )
{
    if( src.get_token() == dst.get_token() )
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
opcd directory::move_file_from( const zstring& src, const token& name )
{
    _curpath.resize(_baselen);

    if( name.is_empty() )
    {
        //extract name from the source path
        token srct = src.get_token();
        token srcfn = srct.cut_right_back( separator() );
        _curpath << srcfn;
    }
    else
        _curpath << name;
    
    return move_file( src, _curpath );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_file_to( const zstring& dst, const token& name )
{
    _curpath.resize(_baselen);

    if( name.is_empty() )
    {
        //extract name from the destination path
        token dstt = dst.get_token();
        token srcfn = dstt.cut_right_back( separator() );
        _curpath << srcfn;
    }
    else
        _curpath << name;
    
    return move_file( _curpath, dst );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_current_file_to( const zstring& dst )
{
    return move_file( _curpath, dst );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_file( const zstring& src, const zstring& dst )
{
    if( 0 == ::rename(src.c_str(), dst.c_str()) )
        return 0;
    return ersIO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::delete_file( const zstring& src )
{
#ifdef SYSTYPE_MSVC
    return 0 == _unlink(src.c_str())  ?  opcd(0) : ersIO_ERROR;
#else
    return 0 == unlink(src.c_str()) ? opcd(0) : ersIO_ERROR;
#endif
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::delete_directory( const zstring& src )
{
#ifdef SYSTYPE_MSVC
    return 0 == _rmdir(src.c_str())  ?  opcd(0) : ersIO_ERROR;
#else
    return 0 == rmdir(src.c_str()) ? opcd(0) : ersIO_ERROR;
#endif
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::set_file_times( const zstring& fname, timet actime, timet modtime )
{
#ifdef SYSTYPE_WIN
#if _MSC_VER == 1800
    //fix for VS2013 bug
    // http://connect.microsoft.com/VisualStudio/feedback/details/811534/utime-sometimes-fails-to-set-the-correct-file-times-in-visual-c-2013
    long tzo;
    _get_dstbias(&tzo);
    actime.t -= tzo;
    modtime.t -= tzo;
#endif

    __utimbuf64 ut;
    ut.actime = actime;
    ut.modtime = modtime;
    return _utime64(fname.c_str(), &ut) ? ersFAILED : ersNOERR;
#else
#error TODO
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_writable( const zstring& fname )
{
    return 0 == _access(fname.c_str(), 2);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::set_writable( const zstring& fname, bool writable )
{
    return 0 == _chmod(fname.c_str(), writable ? (_S_IREAD | _S_IWRITE) : _S_IREAD);
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
opcd directory::mkdir_tree( token name, bool last_is_file, uint mode )
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
            pc[i] = c;

            if(e)  return e;
        }
    }

    return last_is_file ? ersNOERR : mkdir(path, mode);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::get_relative_path( token src, token dst, charstr& relout )
{
#ifdef SYSTYPE_WIN
    bool sf = src.nth_char(1) == ':';
    bool df = dst.nth_char(1) == ':';
#else
    bool sf = src.first_char() == '/';
    bool df = dst.first_char() == '/';
#endif

    if(sf != df) return false;

    if(sf) {
#ifndef SYSTYPE_WIN
        src.shift_start(1);
        dst.shift_start(1);
#endif
    }

    if(directory::is_separator(src.last_char()))
        src.shift_end(-1);

    while(1)
    {
        token st = src.cut_left(DIR_SEPARATORS);
        token dt = dst.cut_left(DIR_SEPARATORS);

#ifdef SYSTYPE_WIN
        if(!st.cmpeqi(dt)) {
#else
        if(st!=dt) {
#endif
            src.set(st.ptr(), src.ptre());
            dst.set(dt.ptr(), dst.ptre());
            break;
        }
    }

    relout.reset();
    while(src) {
        src.cut_left(DIR_SEPARATORS);
#ifdef SYSTYPE_WIN
        relout << "..\\";
#else
        relout << "../";
#endif
    }

    return append_path(relout, dst);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::compact_path( charstr& dst, char tosep )
{
    token dtok = dst;

#ifdef SYSTYPE_WIN
    bool absp = dtok.nth_char(1) == ':';
    if(absp) {
        char c2 = dtok.nth_char(2);
        if(c2 != '/' && c2 != '\\' && c2 != 0)
            return false;
        if(!c2)
            return true;
        dst[2] = tosep;
        dtok.shift_start(3);
    }
#else
    bool absp = dtok.first_char() == '/';
    if(absp) {
        if(tosep)
            dst[0] = tosep;
        dtok.shift_start(1);
    }
#endif

    token fwd, rem;
    fwd.set_empty(dtok.ptr());

    static const token up = "..";
    int nfwd=0;

    do {
        token seg = dtok.cut_left(DIR_SEPARATORS);
        bool isup = seg == up;

        int d = dtok.ptr() - seg.ptre();
        if(d > 1) {
            //remove extra path separators
            dst.del(seg.ptre() - dst.ptr(), d-1);
            dtok.shift_start(1-d);
            dtok.shift_end(1-d);
        }

        //normalize path separator
        if(d > 0 && tosep)
            *(char*)seg.ptre() = tosep;

        if(!isup) {
            if(rem.len()) {
                int rlen = rem.len();
                dst.del(int(rem.ptr()-dst.ptr()), rlen);
                dtok.shift_start(-rlen);
                dtok.shift_end(-rlen);
                rem.set_empty();
            }

            //count forward going tokens
            ++nfwd;
            fwd._pte = dtok ? (dtok.ptr()-1) : dtok.ptr();
        }
        else if(nfwd) {
            //remove one token from fwd
            fwd.cut_right_back(DIR_SEPARATORS);
            rem.set(fwd.ptre(), dtok.first_char() ? (dtok.ptr()-1) : dtok.ptr());
            --nfwd;
        }
        else {
            //no more forward tokens, remove rem range
            if(absp)
                return false;

            if(rem.len()) {
                if(rem.ptr() > dst.ptr())
                    rem.shift_start(-1);
                else if(rem.ptre() < dst.ptre())
                    rem.shift_end(1);
                    
                int rlen = rem.len();

                dst.del(int(rem.ptr()-dst.ptr()), rlen);
                dtok.shift_start(-rlen);
                dtok.shift_end(-rlen);
                rem.set_empty();
            }

            fwd._ptr = dtok.ptr();
        }

    }
    while(dtok);

    if(rem.len())
        dst.del(int(rem.ptr()-dst.ptr()), rem.len());

    return true;
}


COID_NAMESPACE_END
