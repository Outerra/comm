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
#include "commtime.h"
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

#ifdef SYSTYPE_MINGW
    typedef struct __stat64 xstat;
#elif defined(SYSTYPE_MSVC)
	typedef struct _stat64 xstat;
#else
    typedef struct stat64 xstat;
#endif

    ~directory();
    directory();


    ///Open directory for iterating files using the filter
    opcd open( token path_and_pattern ) {
        token pattern = path_and_pattern;
        token path = pattern.cut_left_back("\\/", token::cut_trait_remove_sep_default_empty());
        return open(path, pattern);
    }

    ///Open directory for iterating files using the filter
    opcd open( const token& path, const token& filter );

    ///Open the current directory for iterating files using the filter
    opcd open_cwd( const token& filter )
    {
        charstr path;
        return open( get_cwd(path), filter );
    }
    void close();

    ///@return true if the character is allowed path separator
    ///@note on windows it's both / and \ characters
    static bool is_separator( char c )      { return c == '/'  ||  c == separator(); }

    static char separator();
    static const char* separator_str();
    static charstr& treat_trailing_separator( charstr& path, bool shouldbe )
    {
        char c = path.last_char();
        if( is_separator(c) ) {
            if(!shouldbe)  path.resize(-1);
        }
        else if(shouldbe && c!=0)   //don't add separator to an empty path, that would make it absolute
            path.append( separator() );
        return path;
    }

#ifdef SYSTYPE_WIN
#define DIR_SEPARATORS "\\/"
#define DIR_SEPARATOR_STRING "\\"
#else
#define DIR_SEPARATORS '/'
#define DIR_SEPARATOR_STRING "/"
#endif

    bool is_entry_open() const;
    bool is_entry_directory() const;
    bool is_entry_regular() const;

    static bool is_valid( const zstring& dir );

    //@return true if path is a directory
    static bool is_valid_directory( const zstring& arg );

    static bool is_valid_file( const zstring& arg );

    static bool is_directory( ushort mode );
    static bool is_regular( ushort mode );

    static bool stat( const zstring& name, xstat* dst );
    
    static opcd mkdir( const zstring& name, uint mode=0750 );

    static opcd mkdir_tree( token name, bool last_is_file=false, uint mode=0750 );

    static int chdir( const zstring& name );

    static opcd delete_directory( const zstring& src );

    static opcd copy_file( const zstring& src, const zstring& dst );

    static opcd move_file( const zstring& src, const zstring& dst );

    static opcd delete_file( const zstring& src );

    ///delete multiple files using a pattern for file
    static opcd delete_files( token path_and_pattern );

    ///copy file to open directory
    opcd copy_file_from( const token& src, const token& name=token() );

    opcd copy_file_to( const token& dst, const token& name=token() );
    opcd copy_current_file_to( const token& dst );


    ///move file to open directory
    opcd move_file_from( const zstring& src, const token& name=token() );

    opcd move_file_to( const zstring& dst, const token& name=token() );
    opcd move_current_file_to( const zstring& dst );

    static opcd set_file_times( const zstring& fname, timet actime, timet modtime );

    ///Get current working directory
    static charstr& get_cwd( charstr& buf );

    ///Get program executable directory
    static charstr& get_ped( charstr& buf );

    ///Get temp directory
    static charstr& get_tmp( charstr& buf );

    ///Get current program file path
    static charstr& get_program_path( charstr& buf );

    ///Get user home directory
    static charstr& get_home_dir( charstr& buf );

    ///Get relative path from src to dst
    static bool get_relative_path( token src, token dst, charstr& relout );

    ///Append \a path to destination buffer. If the path is absolute, previous
    /// content of \a dst is dicarded.
    static bool append_path( charstr& dst, token path );

    static bool is_absolute_path( const token& path );

    ///Remove nested ../ chunks
    static bool compact_path( charstr& dst );


    uint64 file_size() const                    { return _st.st_size; }
    static uint64 file_size( const zstring& file );

    ///Get next entry in the directory
	const xstat* next();

    const xstat* get_stat() const               { return &_st; }

    ///After a successful call to next(), this function returns full path to the file
    const charstr& get_last_full_path() const   { return _curpath; }
    token get_last_dir() const                  { return token( _curpath.ptr(), _baselen ); }

    const char* get_last_file_name() const      { return _curpath.c_str() + _baselen; }
    token get_last_file_name_token() const      { return token(_curpath.c_str()+_baselen,_curpath.len()-_baselen); }

private:
    charstr     _curpath;
    uint        _baselen;
    xstat 		_st;
    charstr     _pattern;

#ifdef SYSTYPE_MSVC
    ints        _handle;
#else
	DIR*        _dir;
#endif

};

COID_NAMESPACE_END

#endif //__COID_COMM_DIR__HEADER_FILE__
