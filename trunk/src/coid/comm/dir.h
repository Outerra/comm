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

#ifndef __COID_COMM_DIR__HEADER_FILE__
#define __COID_COMM_DIR__HEADER_FILE__

#include "namespace.h"

#include "retcodes.h"
#include <sys/stat.h>
#include "str.h"

#ifndef SYSTYPE_MSVC
# include <dirent.h>
#else
# include <direct.h>
#endif

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
class directory
{
public:

    ~directory();
    directory();

	opcd open( const token& path, const token& filter );
    opcd open_cwd( const token& filter )
    {
        charstr path;
        return open( get_cwd(path), filter );
    }
    void close();


    static char separator();
    static const char* separator_str();
    static charstr& treat_trailing_separator( charstr& path, bool shouldbe )
    {
        if( path.last_char() == separator() ) {
            if(!shouldbe)  path.trim_to_length(-1);
        }
        else if(shouldbe)
            path.append( separator() );
        return path;
    }

    bool is_entry_open() const;
    bool is_entry_directory() const;
    bool is_entry_regular() const;

    static bool is_valid( const charstr& dir )              { return is_valid( dir.ptr() ); }
    static bool is_valid( const char* dir )
    {
        struct stat st;
        return 0 == stat( dir, &st );
    }

    static bool is_valid_directory( const charstr& arg )    { return is_valid_directory( arg.ptr() ); }
    static bool is_valid_directory( const char* arg )
    {
        struct stat st;
        return 0 == stat( arg, &st )  &&  is_directory(st.st_mode);
    }

    static bool is_valid_file( const charstr& arg )         { return is_valid_file( arg.ptr() ); }
    static bool is_valid_file( const char* arg )
    {
        struct stat st;
        return 0 == stat( arg, &st )  &&  is_regular(st.st_mode);
    }

    static bool is_directory( ushort mode );
    static bool is_regular( ushort mode );
    static opcd mkdir( const charstr& name, uint mode=0750 );
	static int chdir( const charstr& name );

    static opcd copy_file( const token& src, const token& dst );
    static opcd move_file( const charstr& src, const charstr& dst );
    static opcd delete_file( const charstr& src );

    ///copy file to open directory
    opcd copy_file_from( const token& src, const token& name=token::empty() );

    opcd copy_file_to( const token& dst, const token& name=token::empty() );
    opcd copy_current_file_to( const token& dst );


    ///move file to open directory
    opcd move_file_from( const token& src, const token& name=token::empty() );

    opcd move_file_to( const token& dst, const token& name=token::empty() );
    opcd move_current_file_to( const token& dst );


    static charstr& get_cwd( charstr& buf );

    ///Get next entry in the directory
	const struct stat* next();

    const struct stat* get_stat() { return &_st; }

    ///After a successful call to next(), this function returns full path to the file
    const charstr& get_last_full_path() const   { return _curpath; }
    const char* get_last_file_name() const      { return _curpath.ptr() + _baselen; }
    token get_last_file_name_token() const      { return token(_curpath.ptr()+_baselen,_curpath.len()-_baselen); }

private:
    charstr     _curpath;
    uints       _baselen;
    struct stat _st;
    charstr     _pattern;

#ifdef SYSTYPE_MSVC
    long        _handle;
#else
	DIR*        _dir;
#endif

};

COID_NAMESPACE_END

#endif //__COID_COMM_DIR__HEADER_FILE__
