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
opcd directory::copy_file_from( const token& src, const token& name )
{
    _curpath.trim_to_length(_baselen);

    if( name.is_empty() )
    {
        //extract name from the source path
        token srct = src;
        token srcfn = srct.cut_right_back( separator(), 0 );
        _curpath << srcfn;
    }
    else
        _curpath << name;
    
    return copy_file( src, _curpath );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_file_to( const token& dst, const token& name )
{
    _curpath.trim_to_length(_baselen);

    if( name.is_empty() )
    {
        //extract name from the destination path
        token dstt = dst;
        token srcfn = dstt.cut_right_back( separator(), 0 );
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
    fileiostream fsrc, fdst;
    charstr argdst;

    opcd e = fsrc.open(src);
    if(e)
        return e;

    argdst << dst << "?wct";
    e = fdst.open(argdst);
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
    _curpath.trim_to_length(_baselen);

    if( name.is_empty() )
    {
        //extract name from the source path
        token srct = src;
        token srcfn = srct.cut_right_back( separator(), 0 );
        _curpath << srcfn;
    }
    else
        _curpath << name;
    
    return move_file( src, _curpath );
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_file_to( const token& dst, const token& name )
{
    _curpath.trim_to_length(_baselen);

    if( name.is_empty() )
    {
        //extract name from the destination path
        token dstt = dst;
        token srcfn = dstt.cut_right_back( separator(), 0 );
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
opcd directory::move_file( const charstr& src, const charstr& dst )
{
    if( 0 == rename( src.ptr(), dst.ptr() ) )
        return 0;
    return ersIO_ERROR;
}

COID_NAMESPACE_END
