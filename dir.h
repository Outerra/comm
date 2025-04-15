#pragma once
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
 * Portions created by the Initial Developer are Copyright (C) 2003-2023
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

#include "namespace.h"

#include "retcodes.h"
#include "commtime.h"
#include <sys/stat.h>
#include "str.h"
#include "trait.h"
#include "function.h"

#ifndef SYSTYPE_MSVC
# include <dirent.h>
#else
# include <direct.h>
#endif

COID_NAMESPACE_BEGIN

#ifdef SYSTYPE_WIN
static constexpr token DIR_SEPARATORS = "\\/"_T;
static constexpr token DIR_SEPARATOR_STRING = "\\"_T;
#else
static constexpr char DIR_SEPARATORS = '/';
static constexpr token DIR_SEPARATOR_STRING = "/"_T;
#endif

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
    opcd open(token path_and_pattern) {
        token pattern = path_and_pattern;
        token path = pattern.cut_left_group_back("\\/", token::cut_trait_remove_sep_default_empty());
        return open(path, pattern);
    }

    ///Open directory for iterating files using the filter
    opcd open(const token& path, const token& filter);

    ///Open the current directory for iterating files using the filter
    opcd open_cwd(const token& filter)
    {
        return open(get_cwd(), filter);
    }
    void close();

    /// @return true if the character is allowed path separator
    /// @note on windows it's both / and \ characters
    static bool is_separator(char c) { return c == '/' || c == separator(); }

    static char separator();
    static const char* separator_str();

#ifdef SYSTYPE_WIN
    static const token& separators() { static token sep = "/\\"_T; return sep; }
#else
    static const token& separators() { static token sep = "/"_T; return sep; }
#endif

    /// @param shouldbe true or one of / or \ characters if path should end with the separator, false if path separator should not be there
    static charstr& treat_trailing_separator(charstr& path, char shouldbe)
    {
        if (shouldbe && shouldbe != '/' && shouldbe != '\\')
            shouldbe = separator();

        char c = path.last_char();
        if (is_separator(c)) {
            if (!shouldbe)  path.resize(-1);
        }
        else if (shouldbe && c != 0)   //don't add separator to an empty path, that would make it absolute
            path.append(shouldbe);
        return path;
    }

    /// @brief Check if name is valid file or directory name
    /// @param name - name to check
    /// @return true if the name is valid
    static bool is_valid_name(const coid::token& name);

    /// @brief Validates filename
    /// @param filename(in/out) - name to validate 
    /// @param replacement_char - replacement character for the invalid characters found in the filename
    /// @return validated name
    static charstr& validate_filename(charstr& filename, char replacement_char = '_') {
        if (!is_valid_name_char(replacement_char))
        {
            DASSERTX(0, "replacement character invalid");
            replacement_char = '_';
        }

        coid::token tok = coid::token(filename);
        const char* s = tok._ptr;
        const char* e = tok._pte;
        for (const char* i = s; i != e; i++) {
            if (!is_valid_name_char(*i))
            {
                filename[i - s] = replacement_char;
            }
        }

        if (tok.char_is_whitespace(tok.len() - 1) || tok.last_char() == '.')
        {
            filename[tok.len() - 1] = replacement_char;
        }

        DASSERT(is_valid_name(filename));

        return filename;
    }

    enum class verify_path_syntax_result_enum : uint8
    {
        invalid = 0,
        valid_relative_directory_path,
        valid_absolue_directory_path,
        valid_relative_file_path,
        valid_absolute_file_path,
    };
    
    /// @brief Checks whether the given path is syntactically valid (i.e., uses only allowed characters, format, etc.).
    /// @param path The path to validate.
    /// @return One of the following:
    ///         - `invalid`: The path is not syntactically valid.
    ///         - `valid_relative_directory_path`: A valid relative path to a directory.
    ///         - `valid_absolute_directory_path`: A valid absolute path to a directory.
    ///         - `valid_relative_file_path`: A valid relative path to a file.
    ///         - `valid_absolute_file_path`: A valid absolute path to a file.
    /// @note This function only verifies the path's syntax; it does not check if the file or directory exists on the device.
    static verify_path_syntax_result_enum verify_path_syntax(const coid::token& path);

    bool is_entry_open() const;
    bool is_entry_directory() const;
    bool is_entry_subdirectory() const;     //< a directory, but not . or ..
    bool is_entry_regular() const;

    /// @brief Checks whether the given path exists on the physical device and determines its type.
    /// @param path The path to the file or directory to validate.
    /// @return 
    ///         - `0` if the path does not exist on the device (invalid),
    ///         - `1` if the path exists and points to a file,
    ///         - `2` if the path exists and points to a directory.
    /// @note Unlike the syntax check, this function verifies the actual existence of the path on the file system.
     static int is_valid(zstring path);

     /// @brief Checks whether the given directory path exists
     /// @param path The path to the directory to validate.
     /// @return true if exists, false otherwise
    /// @note Unlike the syntax check, this function verifies the actual existence of the path on the file system.

    static bool is_valid_directory(zstring arg);

    /// @brief Checks whether the given file path exists
    /// @param path The path to the file to validate.
    /// @return true if exists, false otherwise
    /// @note Unlike the syntax check, this function verifies the actual existence of the path on the file system.
    static bool is_valid_file(zstring arg);

    static bool is_directory(ushort mode);
    static bool is_regular(ushort mode);

    static bool stat(zstring name, xstat* dst);

    static opcd mkdir(zstring name, uint mode = 0750);

    static opcd mkdir_tree(token name, bool last_is_file = false, uint mode = 0750);

    static int chdir(zstring name);

    static opcd delete_directory(zstring src, bool recursive);

    ///Move directories or files, also works across drives
    /// @note will fail if src file already exists in dst
    static opcd move_directory(zstring source, zstring destination) 
    {
        if (!is_valid_directory(source) || !is_valid_directory(destination))
        {
            return ersINVALID_PARAMS;
        }
        
        return copymove_directory(source, destination, true);
    }

    enum class copy_directory_mode_enum
    {
        contents_only,  //< copies only the inner contents of the source directory
        whole_directory //< copies the entire directory, including the source folder itself
    };

    
    /// @brief Copies source dirctory to destination directory
    /// @param source - source directory path
    /// @param destination - destination directory path
    /// @param mode - see directory_copy_mode_enum 
    /// @return error code 
    /// @note will fail if source directory or file already exists in destination directory
    static opcd copy_directory(zstring source, zstring destination, copy_directory_mode_enum mode)
    {
        if (!is_valid_directory(source) || !is_valid_directory(destination))
        {
            return ersINVALID_PARAMS;
        }
        

        coid::charstr& src_str = source.get_str();
        coid::charstr& dst_str = destination.get_str();
        treat_trailing_separator(src_str, false);
        
        if (mode == copy_directory_mode_enum::whole_directory)
        {
            treat_trailing_separator(dst_str, '/');

        }
        else if (mode == copy_directory_mode_enum::contents_only)
        {
            treat_trailing_separator(dst_str, false);
        }
        else 
        {
            DASSERT_RETX(0, "Not implemented. New copy_directory_mode_enum value added?", ersFAILED_ASSERTION);
        }

        return copymove_directory(source, destination, false);
    }

    static opcd rename_directory(zstring path, zstring new_name);

    static opcd copy_file(zstring src, zstring dst, bool preserve_dates);

    /// @brief Move file
    /// @param src source path (absolute or relative to the working dir)
    /// @param dst target path (absolute or relative to the working dir)
    /// @param replace_existing true if an existing target should be replaced
    static opcd move_file(zstring src, zstring dst, bool replace_existing);

    /// @brief Rename file, target is relative to the source path
    /// @param src source path (absolute or relative to the working dir)
    /// @param dst target path (absolute or relative to the source path)
    /// @param replace_existing true if an existing target should be replaced
    static opcd rename_file(zstring src, zstring dst, bool replace_existing);

    static opcd delete_file(zstring src);

    ///delete multiple files using a pattern for file
    static opcd delete_files(token path_and_pattern);

    ///Copy file to the open directory
    opcd copy_file_from(const token& src, bool preserve_dates, const token& name = token());

    ///Copy current file to dst dir path
    /// @param dst destination directory path
    /// @param preserve_dates use access and modification times of the source file
    /// @param name optional file name, if it's different than the current one
    opcd copy_file_to(const token& dst, bool preserve_dates, const token& name = token());
    opcd copy_current_file_to(const token& dst, bool preserve_dates);


    ///move file to open directory
    opcd move_file_from(zstring src, const token& name = token(), bool replace_existing = false);

    opcd move_file_to(zstring dst, const token& name = token(), bool replace_existing = false);
    opcd move_current_file_to(zstring dst, bool replace_existing);

    /// Set file times
    /// @param fname file name to modify
    /// @param actime access time, 0 if not changed
    /// @param modtime modification time, 0 if not changed
    /// @param crtime creation time, 0 if not changed, ignored on linux
    static opcd set_file_times(zstring fname, timet actime, timet modtime, timet crtime = 0);

    static opcd truncate(zstring fname, uint64 size);

    /// @{ tests and sets file write access flags
    static bool is_writable(zstring fname);
    static bool set_writable(zstring fname, bool writable);
    /// @}

    ///Get current working directory
    static charstr get_cwd();
    static charstr& get_cwd(charstr& buf) {
        return buf = get_cwd();
    }

    ///Get program executable directory
    static charstr get_program_dir() {
        charstr buf = get_program_path();

        token t = buf;
        t.cut_right_group_back(separators(), token::cut_trait_keep_sep_with_source_default_full());

        return buf.resize(t.len());
    }
    static charstr& get_program_dir(charstr& buf) {
        return buf = get_program_dir();
    }

    ///Get current program executable file path
    static charstr get_program_path();

    ///Get program executable directory
    static charstr get_module_dir() {
        charstr buf = get_module_path();

        token t = buf;
        t.cut_right_group_back(separators(), token::cut_trait_keep_sep_with_source_default_full());

        return buf.resize(t.len());
    }

    ///Get current module file path, or module path where given function resides
    /// @param func pointer to a function
    static charstr get_module_path(const void* func = 0) {
        charstr buf;
        get_module_path_func(func ? func : (const void*)&dummy_func, buf, false);
        return buf;
    }

    /// @brief Get module path (from function address or current)
    /// @param dst target path
    /// @param append true append to dst, false set to dst
    /// @param func optional function pointer to get the module handle for
    /// @return module handle
    static uints get_module_path(charstr& dst, bool append = false, const void* func = 0) {
        return get_module_path_func(func ? func : (const void*)&dummy_func, dst, append);
    }

    ///Get current module handle, or module handle where given function resides
    static uints get_module_handle(const void* func = 0) {
        return get_module_handle_func(func ? func : (const void*)&dummy_func);
    }


    ///Get temp directory
    static charstr get_tmp_dir();

    ///Create temp directory in system temp folder
    /// @param prefix prefix name to use
    static charstr create_tmp_dir(const token& prefix);

    ///Get user home directory
    static charstr get_home_dir();

    ///Get relative path from src to dst
    static bool get_relative_path(token src, token dst, charstr& relout, bool last_src_is_file = false);

    ///Append \a path to the destination buffer
    /// @param dst path to append to, also receives the result
    /// @param path relative path to append; an absolute path replaces the content of dst
    /// @param keep_below if true, only allows relative paths that cannot get out of the input path
    /// @return >0 ok, 0 error, <0 if path is ok but possibly outside of input directory
    static int append_path(charstr& dst, token path, bool keep_below = false);

    static bool is_absolute_path(const token& path);

    /// @return true if path is under or equals root
    /// @note paths must be compact
    static bool is_subpath(token root, token path);

    /// @return true if path is under or equals root, if true path is modified to contain the relative path
    /// @note paths must be compact
    static bool subpath(token root, token& path);

    ///Remove nested ../ chunks, remove extra path separator characters
    /// @param tosep replace separators with given character (usually '/' or '\\')
    static bool compact_path(charstr& dst, char tosep = 0);


    uint64 file_size() const { return _st.st_size; }
    static uint64 file_size(zstring file);

    uint64 calculate_directory_size() { return calculate_directory_size(_curpath); }
    static uint64 calculate_directory_size(const coid::token& path);


    ///Get next entry in the directory
    const xstat* next();

    const xstat* get_stat() const { return &_st; }

    ///After a successful call to next(), this function returns full path to the file
    const charstr& get_last_full_path() const { return _curpath; }
    token get_last_dir() const { return token(_curpath.ptr(), _baselen); }

    const char* get_last_file_name() const { return _curpath.c_str() + _baselen; }
    token get_last_file_name_token() const { return token(_curpath.c_str() + _baselen, _curpath.len() - _baselen); }

    enum class recursion_mode
    {
        immediate_files,            //< no recursion into sundirectories, list only files
        immediate_files_and_dirs,   //< no recursion into subdirectories, list files and dirs
        immediate_dirs_only,        //< no recursion into subdirectories, list dirs only
        recursive_files,            //< list files while recursively entering subdirs, callbacks only for files
        recursive_dirs_only,        //< list directories while recursively entering subdirs
        recursive_dirs_exit,        //< list files while recursively entering subdirs, subdir callback invoked after listing the subdir content
        recursive_dirs_enter,       //< list files while recursively entering subdirs, subdir callback invoked before listing the subdir content
        recursive_dirs_enter_exit,  //< list files while recursively entering subdirs, subdir callback invoked both before and after listing the subdir content
    };

    enum class list_entry
    {
        file,
        dir_exit,
        dir_enter,
    };

    ///Lists all files with extension (extension = "" or "*" if all files) in directory with path using func functor.
    ///Lists also subdirectories paths when recursive_flags set
    /// @param mode - nest into subdirectories and calls callback fn in order specified by recursive_mode
    /// @param fn callback function(const charstr& file_path, recursion_mode mode)
    /// @note fn callback recursion_mode parameter invoked on each file or on directories upon entering or exisitng (or both)
    static bool list_file_paths(const token& path, const token& extension, recursion_mode mode,
        const coid::function<void(const charstr&, list_entry)>& fn);

    ///structure returned by ::find_files
    struct find_result {
        coid::token _path;              //< temporary! => do not store this token, make string copy if you need to store it
        uint64      _size;              //< in bytes, always 0 for directories
        time_t      _last_modified;     //< unix time
        uint        _flags;             //< windows only! always 0 in gcc build

        enum flags {
            readonly = 0x00000001,
            hidden = 0x00000002,
            system = 0x00000004,
            directory = 0x00000010,
            encrypted = 0x00004000
        };
    };

    ///lists all files with given extension and their "last modified" times
    ///note: does not return any folder paths
    /// @param path where to search
    /// @param extension only files whose paths end with this token are returned. keep empty to find all files
    /// @param recursive if true subfolders will be recursively searched
    /// @param return_also_folders if true the callbeck will be called also for folders (even when searching for files with extension)
    /// @param fn callback function called for each found file
    static void find_files(
        const token& path,
        const token& extension,
        bool recursive,
        bool return_also_folders,
        const coid::function<void(const find_result& file_info)>& fn);

    /// @brief Check if directory is empty
    /// @param directory_path - path (absolute or relative to the working directory)
    /// @return true when valid and empty directory, false otherwise
    static bool is_directory_empty(const coid::token& directory_path);

protected:
    static bool is_valid_name_char(char c)
    {
        static char forbidden_chars[] = { '\\','/',':', '*', '?','\"','<', '>', '|' };
        
        return c != forbidden_chars[0] &&
            c != forbidden_chars[1] &&
            c != forbidden_chars[2] &&
            c != forbidden_chars[3] &&
            c != forbidden_chars[4] &&
            c != forbidden_chars[5] &&
            c != forbidden_chars[6] &&
            c != forbidden_chars[7] &&
            c != forbidden_chars[8];
    }


    static opcd copymove_directory(zstring src, zstring dst, bool move);

    static bool is_valid_dir(const char* path);

    static void dummy_func() {
    }

    static const char* no_trail_sep(zstring& name);

    /// @return handle of module where fn resides
    static uints get_module_handle_func(const void* fn);

    /// @param dst string buffer to receive module path
    /// @return handle of module where fn resides
    static uints get_module_path_func(const void* fn, charstr& dst, bool append);

private:
    charstr _curpath;
    charstr _pattern;
    xstat _st;
    uint _baselen = 0;

#ifdef SYSTYPE_MSVC
    ints _handle = 0;
#else
    DIR* _dir = 0;
#endif

};

COID_NAMESPACE_END
