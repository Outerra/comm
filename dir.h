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

///Tests of coid::directory, befriended as a whole so they can reach the parts of it that are
///not public, see the friend declaration in @ref coid::directory
struct directory_tests;

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
    friend struct ::directory_tests;
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
        token path = pattern.cut_left_group_back(DIR_SEPARATORS, token::cut_trait_remove_sep_all_default_empty());
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
    static constexpr token_literal separators() { constexpr token_literal sep = "/\\"_T; return sep; }
#else
    static constexpr token_literal separators() { constexpr token_literal sep = "/"_T; return sep; }
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
    ///         - `valid_absolue_directory_path`: A valid absolute path to a directory.
    ///         - `valid_relative_file_path`: A valid relative path to a file.
    ///         - `valid_absolute_file_path`: A valid absolute path to a file.
    /// @note This function only verifies the path's syntax; it does not check if the file or directory exists on the device.
    /// @note A path is absolute when it starts with a root component, which is "C:" for a drive
    ///       path and "\\\\server" for a unc path, the latter written with either separator style,
    ///       "\\\\server\\share" and "//server/share" alike. @see extract_path_component
    /// @note The server of a unc root, the share that may follow it and every component below are
    ///       all validated as names. A path holding only the root, or only the root and the share,
    ///       denotes a directory, the same way a bare drive does.
    /// @note On non windows systems a path is absolute when it starts at the root separator
    ///       ("/...")
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

    /// @brief Specifies how a directory move operation handles the target path.
    enum class move_directory_mode_enum
    {
        move_to,    ///< Moves the source directory into the target directory.
                    ///< @example src: "X:/src/", dst: "X:/dst/" -> result: "X:/dst/src/"
        rename      ///< Moves and renames the source directory to match the target path.
                    ///< @example src: "X:/src/", dst: "X:/dst/" -> result: "X:/dst/"
    };

    /// @brief Moves a directory to a new location, including across disk volumes.
    ///
    /// @param source Path to the source directory.
    /// @param destination Path to the destination directory.
    /// @param mode Operation mode (@see move_directory_mode_enum).
    ///
    /// @return `opcd` Error code indicating success or the specific cause of failure.
    ///
    /// @note **Mode Behavior & Failure Cases:**
    /// - @ref move_directory_mode_enum::move_to "move_to":
    ///   Fails if either @p source or @p destination is not a valid directory, or if
    ///   @p destination already contains a directory with the same name as @p source.
    /// - @ref move_directory_mode_enum::rename "rename":
    ///   Fails if @p source is not a valid directory, or if the @p destination path already exists.
    static opcd move_directory(zstring source, zstring destination, move_directory_mode_enum mode)
    {
        if (!is_valid_directory(source))
            return ersINVALID_PARAMS;

        if (mode == move_directory_mode_enum::move_to && !is_valid_directory(destination))
            return ersINVALID_PARAMS;

        if (mode == move_directory_mode_enum::rename && is_valid_directory(destination))
            return ersALREADY_EXISTS;

        coid::charstr& src_str = source.get_str();
        coid::charstr& dst_str = destination.get_str();

        if (mode == move_directory_mode_enum::move_to)
        {
            //src without trailing separator, dst with trailing separator
            //-> copymove_directory moves the whole src dir into dst/<srcname>/
            treat_trailing_separator(src_str, false);
            treat_trailing_separator(dst_str, '/');


            coid::charstr check = dst_str;

            uint32 root_length = 0;
            coid::token dst_dir;
            coid::token remainder;
            const bool valid = extract_path_component_internal(source.get_token(), root_length, path_component_enum::last, dst_dir, remainder);
            DASSERTX(valid, "dst_dir is not valid. How?");

            check << dst_dir;

            if (is_valid_directory(check))
                return ersALREADY_EXISTS;
        }
        else if (mode == move_directory_mode_enum::rename)
        {
            //neither src nor dst have a trailing separator
            //-> copymove_directory moves/renames src directly into dst
            treat_trailing_separator(src_str, false);
            treat_trailing_separator(dst_str, false);
        }
        else
        {
            DASSERT_RETX(0, "Not implemented. New move_directory_mode_enum value added?", ersFAILED_ASSERTION);
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
    /// @note Will fail if source directory already exists in destination directory
    static opcd copy_directory(zstring source, zstring destination, copy_directory_mode_enum mode)
    {
        if (!is_valid_directory(source) || !is_valid_directory(destination))
        {
            return ersINVALID_PARAMS;
        }


        coid::charstr& src_str = source.get_str();
        coid::charstr& dst_str = destination.get_str();
        treat_trailing_separator(dst_str, '/');


        if (mode == copy_directory_mode_enum::whole_directory)
        {
            treat_trailing_separator(src_str, false);

        }
        else if (mode == copy_directory_mode_enum::contents_only)
        {
            treat_trailing_separator(src_str, '/');
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

    /// @brief Appends a relative or absolute path to a destination buffer.
    /// @param[in,out] dst  The destination path buffer; receives the resolved result.
    /// @param[in]     path The path component (relative or absolute) to append or apply.
    /// @param[in]     keep_below If true, the operation is carried out only when the resulting
    ///                           path stays below the initial directory scope of @p dst.
    ///
    /// @return True when @p dst received the path, false only when @p keep_below is set and the
    ///     result would not lie below @p dst.
    /// @note An absolute @p path replaces the contents of @p dst, a relative one is appended to it.
    ///     With @p keep_below set, either is applied only when the result really lies below @p dst.
    /// @note @p dst is modified on success only, so the buffer of a failed call stays usable.
    /// @note With @p keep_below set, parent segments are allowed as long as the resolved path ends
    ///     up below @p dst again, which requires the components leading back to match the ones the
    ///     parent segments cut off. Component names are compared the way the platform compares them.
    /// @note The two are joined with the OS default separator, and only when @p dst does not
    ///     already end with one. @p path keeps the separators it is written with.
    /// @note @p keep_below compacts the result, the test against @p dst needing it resolved anyway.
    ///     Without it nothing is resolved, @p dst receives @p path as it was given.
    static bool append_path(charstr& dst, token path, bool keep_below = false, char compact_separator = 0);

    /// @brief Concatenate a directory path with a relative or absolute file/dir path
    /// @param dir_path directory path to append to
    /// @param rel_or_abs_path relative or absolute path to append, an absolute one replaces the dir_path completely
    /// @param keep_below If true, the operation is carried out only when the resulting
    ///        path stays below the initial directory scope of @p dir_path.
    /// @return concatenated path
    static charstr concatenate_path(const token& dir_path, const token& rel_or_abs_path, bool keep_below = false, char compact_separator = 0)
    {
        charstr dst = dir_path;
        if (!append_path(dst, rel_or_abs_path, keep_below, compact_separator))
            dst.reset();
        return dst;
    }

    /// @brief Builds a path from a base path and a path component appended to it.
    /// @param[in] base The base path the component is appended to.
    /// @param[in] path The path component (relative or absolute) to append or apply.
    /// @param[in] keep_below If true, the path is built only when the result stays below the
    ///                       directory scope of @p base.
    ///
    /// @return The resolved path, empty when @p path is malformed or when @p keep_below is set
    ///     and the result would not lie below @p base.
    /// @note Convenience wrapper around @ref append_path for the common case of building
    ///     a new path instead of extending an existing buffer. The failure of the two is
    ///     collapsed into an empty result, use @ref append_path when the cause matters.
    static charstr make_path(const token& base, token path, bool keep_below = false)
    {
        charstr dst = base;

        if (!append_path(dst, path, keep_below))
            dst.reset();

        return dst;
    }

    static bool is_absolute_path(const token& path);

    /// @return true if path is under or equals root
    /// @note paths must be compact
    static bool is_subpath(token root, token path);

    static bool is_same_path(coid::token arg0, coid::token arg1);

    /// @return true if path is under or equals root, if true path is modified to contain the relative path
    /// @note paths must be compact
    static bool subpath(token root, token& path);

    /// @brief Resolve the dot segments of a path and collapse the separator runs, in place
    /// @param [in,out] dst Path to compact, rewritten in place. An empty one is not an error.
    /// @param use_separator Replace the separators that are kept with this character (usually '/'
    ///     or '\\'). Pass 0 to keep them as they are written.
    /// @return False when the path is malformed, a drive letter followed by anything but a
    ///     separator ("C:a"), or when it resolves above the root of an absolute path. @p dst is
    ///     left partially compacted then, it is not restored.
    /// @note A parent dir segment consumes the component in front of it, a current dir segment
    ///     resolves to nothing. Only a whole component of two dots is a parent segment, a name may
    ///     start with them - "..b" is an ordinary component and is consumed like any other.
    /// @note A relative path may lead above itself, the parent segments left with nothing to consume
    ///     are kept ("a/../../b" comes out as "../b"). An absolute path has a root to fail against,
    ///     which is what the false return is for.
    /// @note The root is kept: the "C:" drive on windows, the leading separator elsewhere. The two
    ///     leading separators of a unc path are kept the same way, but the server and the share
    ///     below them are ordinary components here, a parent segment consumes them like any other
    ///     ("\\\\server\\..\\x" comes out as "\\\\x"). @see get_path_root_length_internal
    /// @note A trailing separator is preserved, a run of separators anywhere collapses into one.
    static bool compact_path(charstr& dst, char use_separator = 0);

    /// @brief Normalizes a path by resolving relative components and fixing separators.
    /// @details Removes redundant path separators (e.g., `//` -> `/`) and resolves
    /// nested relative segments (e.g., `dir/../`).
    /// @param path The input path string to compact.
    /// @param to_sep The target separator character to normalize to (typically '/' or '\\').
    /// Pass `0` to keep the original separators.
    /// @return The compacted path string, or an empty string if the path is invalid or
    /// escapes the root directory (e.g., `../../` from a root drive).
    static coid::charstr create_compact_path(const coid::token& path, char to_sep = 0);


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

    /// @brief Check if directory is writable
    /// @param directory_path - path (absolute or relative to the working directory)
    /// @return true when path is valid and writable directory, false otherwise
    static bool is_directory_writable(const coid::token& directory_path);

    /// @brief Specifies which path component to extract.
    enum class path_component_enum
    {
        first,  ///< Extract the first path component (leftmost segment).
        last    ///< Extract the last path component (rightmost segment).
    };

    /// @brief Extracts the first or last path component and optionally retrieves the remaining path.
    /// @param[in]      path      Input path (absolute or relative), the empty token once the walk
    ///                           is over, which comes back as an empty component.
    /// @param[in,out]  root_length Length of the root of @p path, the separator run behind it
    ///                           included, zero when it is relative. Pass -1 to have it measured,
    ///                           which is what a caller does for the first call. The call that
    ///                           consumes the root sets it back to zero, so the same variable can
    ///                           be handed to every call of a walk. @see get_path_root_length_internal
    /// @param[in]      component Specifies whether to extract the `first` or `last` component.
    ///                           Defaults to `path_component_enum::last`.
    /// @param[out]     remainder Optional pointer to receive what is left of @p path once the
    ///                           component is taken off, the empty token when nothing is left.
    ///                           Pass `&path` to perform an in-place update.
    /// @param[out] is_root_component Optional pointer that receives true when the call consumed the
    ///                           root of the path, which is the only way to tell an empty root from
    ///                           no component at all.
    /// @return The extracted path component. The empty token when the root is nothing but a
    ///         separator run (the unix root), and when nothing is left to extract.
    ///
    /// @note **Root component:** the root is the first component of the path, "C:" for a drive,
    ///       "\\\\server" for a unc path, and the empty token for a path that starts at the root of
    ///       the current volume. `first` returns it right away and leaves the whole of the path
    ///       below it in @p remainder. `last` cuts below it, the root staying at the head of
    ///       @p remainder until the remainder is the bare root and the next call consumes it.
    ///
    /// @note **Skipping the measuring:** a caller that knows what it is doing may pass zero instead
    ///       of -1, saying the path carries no root the call could reach. That holds for a path
    ///       known to be relative, and for a `last` component taken from a path known not to have
    ///       been walked down to its root yet, the root only being in the way once the walk reaches
    ///       it. The result is undefined when it does reach it: the root is cut into ordinary
    ///       components, "C:" and "\\\\server" alike.
    ///
    /// @note **Separators:** only the separators sitting between the component and @p remainder are
    ///       removed, a whole run of them at once, so neither side ever comes back with one on the
    ///       side the two were split apart. A trailing separator of @p path stays on the remainder
    ///       of a `first` component, a `last` one leaves nothing for it to stay on.
    ///
    /// @note **In-Place Modification:** @p path and @p remainder can safely reference
    ///       the exact same object (e.g., `extract_path_component(p, root, last, &p)`).
    ///
    /// @note **Reassembly:** the components come back without separators, so they concatenate back
    ///       into the path as `root << separator() << component << separator() << ...`, the empty
    ///       root of a volume rooted path being what puts the leading separator back in place.
    ///
    /// @example
    ///   uint32 root = -1;       //ask for the root to be measured
    ///   coid::token rem;
    ///
    ///   // Extract LAST: the separator between the two is removed with it
    ///   auto last = extract_path_component("foo/bar/baz.txt", root, path_component_enum::last, &rem);
    ///   // last == "baz.txt", rem == "foo/bar", root == 0
    ///
    ///   // Extract FIRST: the separator behind the component is removed with it
    ///   root = -1;
    ///   auto first = extract_path_component("foo/bar/baz.txt", root, path_component_enum::first, &rem);
    ///   // first == "foo", rem == "bar/baz.txt"
    ///
    ///   // The root is the first component, and the call consumes it
    ///   bool is_root = false;
    ///   root = -1;
    ///   auto drive = extract_path_component("C:/a/b", root, path_component_enum::first, &rem, &is_root);
    ///   // drive == "C:", rem == "a/b", is_root == true, root == 0
    ///
    ///   // Walking a path down, the same root length carried through it
    ///   root = -1;
    ///   coid::token p = "C:/a/b";
    ///   auto c = extract_path_component(p, root, path_component_enum::last, &p);   // c == "b",   p == "C:/a"
    ///   c = extract_path_component(p, root, path_component_enum::last, &p);        // c == "a",   p == "C:"
    ///   c = extract_path_component(p, root, path_component_enum::last, &p);        // c == "C:",  p == ""
    static coid::token extract_path_component(
        const coid::token& path,
        uint32& root_length,
        path_component_enum component = path_component_enum::last,
        coid::token* remainder = nullptr,
        bool* is_root_component = nullptr
    );

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

    /// @brief Peel the first or the last component off a path whose root has already been measured
    /// @param[in]      path          A valid path, the empty token once the walk is over.
    ///                               @see verify_path_syntax
    /// @param[in, out] root_length   Length of the root of @p path (see get_path_root_length_internal),
    ///                               zero when it is relative. The call that consumes the root sets
    ///                               it back to zero, so the same variable can be handed to every
    ///                               call of a walk. When 0 is passed for a path that does carry a
    ///                               root, the root would be cut into ordinary components.
    /// @param[in]      component     Specifies whether to extract the `first` or the `last` component.
    /// @param[out]     result        The extracted component, the empty token when it the UNIX root.
    /// @param[out]     remainder     Token that left of @p path once the
    ///                               component is taken off, the empty token when nothing is left. May alias @p path.
    /// @return True when result is valid component, False when no valid component left
    /// @note The root is the first component of the path, so a `first` component of a rooted path is
    ///       the root and the remainder is the whole of the path below it, while a `last` component
    ///       is cut below the root, which stays at the head of the remainder until the remainder is
    ///       the bare root and the next call consumes it.
    /// @note Components are always returned without separators with exception of root UNC path on Windows system with two leading separators
    /// @note Remainder are always returned without separator that was between the returned component and remainder
    static bool extract_path_component_internal(
        const coid::token& path,
        uint32& root_length_in_out,
        path_component_enum component,
        coid::token& result,
        coid::token& remainder
    );

    /// @brief Full length of the root of the path, the separator run behind it included
    /// @param[in] path Path to measure the root of, may be empty.
    /// @return The length of the root, zero when @p path is relative.
    static uint32 get_path_root_length_internal(const coid::token& path);

    /// @brief Internal function that appends the path to the result_out while compacting the path
    /// @param [in] path - Path to append. Must be relative. Taken by value, the caller's token is left alone.
    /// @param [in, out] result_in_out - result path. Can't be empty. Must contain at least one path component. Can be relative or absolute. Must be compacted.
    ///     A relative result may end up empty, resolved away by the parent dir segments of @p path.
    /// @param normalize_separators_only - When true, no compacting is performed only the separators are normalized(all the redundant separators are removed). Compacting includes the separator normalization.
    /// @param use_separator - Separator the components are joined with. When 0 the separator is the
    ///     original one that was in front of the component in @p path, a run of them coming down to
    ///     the last one of the sequence. The first component of a relative path has none in front of
    ///     it, the OS default separator is used there. Nothing is written when @p result_in_out
    ///     already ends with a separator.
    /// @param is_result_absolute - if the result is absolute.
    /// @param [in, out] result_regular_component_count_in_out - In: how many regular components the path contains before the call Out: how many regular components the path contains before the call
    /// @return False when compacting fails. Compacting fails when result is absolute and the path is resolved above the root.
    /// @note Only the separators this call writes are subject to @p use_separator, the ones already
    ///     in @p result_in_out are left as they are - the caller normalizes the result beforehand.
    /// @note The root of @p result_in_out is never cut into, and a parent dir segment that resolves
    ///     against nothing is kept as a segment of its own, a relative result having no root to
    ///     fail against. Neither a parent nor a current dir segment counts as a regular component.
    /// @note A trailing separator of @p path is kept on the result, a run of separators anywhere in
    ///     it collapses into the single one the run ends with.
    static bool do_append_compact_internal(token path, charstr& result_in_out, bool normalize_separators_only, char use_separator, bool is_result_absolute, uint32& result_regular_component_count_in_out);


    /// @brief Builds a path from a base path with another path appended to it
    /// @param[in] base_path Base of the result path, may be relative, absolute or empty.
    /// @param[in] appended_path Path appended to the base path, may be relative or absolute.
    ///     An absolute path replaces the base path.
    /// @param[out] result Resulting path, untouched on fail. May alias the buffer that
    ///     @p base_path or @p appended_path point into.
    /// @param[in] make_compact Resolve the current and parent dir segments and collapse the
    ///     separator runs.
    /// @param[in] keep_below Fail when the result would not lie below @p base_path.
    /// @param[in] to_separator Separator to normalize to, typically '/' or '\\'. Pass 0 to keep
    ///     the separators as they are, the two paths are then joined with the last separator
    ///     found in @p base_path, or with the platform one when it holds none.
    /// @return True when @p result received a valid path, false on fail.
    static bool build_path_internal(
        const token& base_path,
        const token& appended_path,
        charstr& result,
        bool make_compact,
        bool keep_below,
        char to_separator = 0);
private:
    static constexpr token_literal CURRENT_DIR_SEGMENT = "."_T;
    static constexpr token_literal PARENT_DIR_SEGMENT = ".."_T;
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
