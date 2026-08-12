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

#ifdef _MSC_VER
#undef __STDC__
#pragma warning(disable:4996)
#endif

#include "dir.h"
#include "binstream/filestream.h"
#include "str.h"
#include "pthreadx.h"

COID_NAMESPACE_BEGIN

#if defined(SYSTYPE_MSVC)
#define xstat64 _stat64
#else
#define xstat64 stat64
#include <unistd.h>
#endif

#ifdef SYSTYPE_WIN
////////////////////////////////////////////////////////////////////////////////

bool is_unc_path(const coid::token& path);

////////////////////////////////////////////////////////////////////////////////

static uint get_unc_root_len(const coid::token& path)
{
    if (path.len() <= 2 || !is_unc_path(path))
    {
        return 0;
    }

    coid::token rest = path;
    rest.shift_start(2);

    const coid::token server = rest.cut_left_group(directory::separators());

    return server.is_empty()
        ? 0
        : uint(server.ptre() - path.ptr());
}
#endif // end of SYSTYPE_WIN

////////////////////////////////////////////////////////////////////////////////
const char* directory::no_trail_sep(zstring& name)
{
    char c = name.get_token().last_char();
    if (c == '\\' || c == '/')
        name.get_str().resize(-1);

    return name.c_str();
}

////////////////////////////////////////////////////////////////////////////////
bool directory::stat(zstring name, xstat* dst)
{
    return 0 == ::xstat64(no_trail_sep(name), dst);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_valid_directory(zstring arg)
{
    token tok = arg.get_token();

    const uint tlen = tok.len();
    bool dosdrive = tlen > 1 && tlen <= 3 && tok[1] == ':';
    bool lastsep = tok.last_char() == '\\' || tok.last_char() == '/';

    if (!dosdrive && lastsep) {
        arg.get_str().resize(-1);
    }
    else if (dosdrive && !lastsep) {
        arg.get_str() << separator();
    }

    return is_valid_dir(arg.c_str());
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_valid_name(const coid::token& name)
{
    // valid name can't contain any forbidden chars  -> see is_valid_name_char function
    // can't end with '.' and whitespaces
    // can't be empty

    if (name.is_empty() || name.last_char() == '.' || name.char_is_whitespace(name.len() - 1))
    {
        return false;
    }

    for (const char* c_ptr = name._ptr;c_ptr != name._pte; ++c_ptr)
    {
        if (!is_valid_name_char(*c_ptr))
        {
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_same_path(coid::token arg0, coid::token arg1)
{
    if (arg0.last_char() == '/' || arg0.last_char() == '\\')
    {
        arg0.shift_end(-1);
    }

    if (arg1.last_char() == '/' || arg1.last_char() == '\\')
    {
        arg1.shift_end(-1);
    }

    coid::token tok0, tok1;

    auto get_next_segment_back = [](coid::token& src_in_out, coid::token& segment)
        {
            uint32 seg_count = 1;
            do
            {
                segment = src_in_out.cut_right_group_back(DIR_SEPARATORS, coid::token::cut_trait_remove_sep_all_default_full());

                if (segment == PARENT_DIR_SEGMENT)
                {
                    ++seg_count;
                }
                else if (segment != CURRENT_DIR_SEGMENT)
                {
                    --seg_count;
                }
            } while (seg_count > 0 && src_in_out.is_set());
        };

    while (arg0.is_set() && arg1.is_set())
    {
        get_next_segment_back(arg0, tok0);
        get_next_segment_back(arg1, tok1);

#ifdef SYSTYPE_WIN
        if (!tok0.cmpeqi(tok1))
        {
            return false;
        }
#else // SYSTYPE_WIN
        if (!tok0.cmpeq(tok1))
        {
            return false;
        }
#endif
    }

    return arg0.is_empty() && arg1.is_empty();
}

////////////////////////////////////////////////////////////////////////////////
charstr directory::create_tmp_dir(const token& prefix)
{
    charstr tmpdir = get_tmp_dir();

    directory::treat_trailing_separator(tmpdir, separator());

    timet now = timet::now();
    int offs = now & 0xffffffff;

    uint n = 10;
    for (uint i = 0; i < n; ++i) {
        tmpdir << prefix << (offs + i);
        if (!directory::is_valid(tmpdir) && directory::mkdir(tmpdir) == NOERR)
            return tmpdir;

        tmpdir.resize(i);
    }

    return charstr();
}

////////////////////////////////////////////////////////////////////////////////
uint64 directory::file_size(zstring file)
{
    if (!file)
        return 0;

    xstat st;
    if (xstat64(file.c_str(), &st) == 0 && is_regular(st.st_mode))
        return st.st_size;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_absolute_path(const token& path)
{
#ifdef SYSTYPE_WIN
    return path.nth_char(1) == ':' || is_unc_path(path);
#else
    return path.first_char() == '/';
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_subpath(token root, token path)
{
    return subpath(root, path);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::append_path(charstr& dst, token path, bool keep_below)
{
    return build_path_internal(dst, path, dst, keep_below, keep_below);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_file_from(const token& src, bool preserve_dates, const token& name)
{
    _curpath.resize(_baselen);

    if (name.is_empty())
    {
        //extract name from the source path
        token srct = src;
        token srcfn = srct.cut_right_back(separator());
        _curpath << srcfn;
    }
    else
        _curpath << name;

    return copy_file(src, _curpath, preserve_dates);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_file_to(const token& dst, bool preserve_dates, const token& name)
{
    _curpath.resize(_baselen);

    if (name.is_empty())
    {
        //extract name from the destination path
        token dstt = dst;
        token srcfn = dstt.cut_right_back(separator());
        _curpath << srcfn;
    }
    else
        _curpath << name;

    return copy_file(_curpath, dst, preserve_dates);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_current_file_to(const token& dst, bool preserve_dates)
{
    return copy_file(_curpath, dst, preserve_dates);
}


////////////////////////////////////////////////////////////////////////////////
opcd directory::rename_directory(zstring path, zstring new_name)
{
    if (!is_valid_directory(path) || !is_valid_name(new_name))
    {
        return ersFAILED;
    }

    coid::token path_tok = path;
    if (is_separator(path_tok.last_char()))
    {
        path_tok.shift_end(-1);
    }
    
    path_tok.cut_right_group_back(DIR_SEPARATORS, coid::token::cut_trait_keep_sep_with_source_default_empty());

    new_name.get_str().ins(0, path_tok);

    return rename_file(path, new_name, false);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copy_file(zstring src, zstring dst, bool preserve_dates)
{
    if (src.get_token() == dst.get_token())
        return 0;

    fileiostream fsrc, fdst;

    opcd e = fsrc.open(src, "r");
    if (e != NOERR)
        return e;

    e = fdst.open(dst, "wct");
    if (e != NOERR)
        return e;

    char buf[8192];
    for (;;)
    {
        uints len = 8192;
        opcd re = fsrc.read_raw(buf, len);
        if (len < 8192)
        {
            uints den = 8192 - len;
            fdst.write_raw(buf, den);
            if (den > 0)
                return ersIO_ERROR "write operation failed";
        }
        else if (re == ersNO_MORE)
            break;
        else
            return re;
    }

    if (preserve_dates) {
        xstat st;
        if (stat(src, &st))
            set_file_times(dst, st.st_atime, st.st_mtime);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_file_from(zstring src, const token& name, bool replace_existing)
{
    _curpath.resize(_baselen);

    if (name.is_empty())
    {
        //extract name from the source path
        token srct = src.get_token();
        token srcfn = srct.cut_right_back(separator());
        _curpath << srcfn;
    }
    else
        _curpath << name;

    return move_file(src, _curpath, replace_existing);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_file_to(zstring dst, const token& name, bool replace_existing)
{
    _curpath.resize(_baselen);

    if (name.is_empty())
    {
        //extract name from the destination path
        token dstt = dst.get_token();
        token srcfn = dstt.cut_right_back(separator());
        _curpath << srcfn;
    }
    else
        _curpath << name;

    return move_file(_curpath, dst, replace_existing);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::move_current_file_to(zstring dst, bool replace_existing)
{
    return move_file(_curpath, dst, replace_existing);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::rename_file(zstring src, zstring dst, bool replace_existing)
{
    if (!is_absolute_path(dst)) {
        zstring target = src.get_token().cut_left_group_back(DIR_SEPARATOR_STRING, token::cut_trait_keep_sep_with_source_default_full());
        append_path(target.get_str(), dst);
        dst.swap(target);
    }

    return move_file(src, dst, replace_existing);
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::delete_file(zstring src)
{
#ifdef SYSTYPE_MSVC
    return 0 == _unlink(src.c_str()) ? opcd(0) : ersIO_ERROR;
#else
    return 0 == unlink(src.c_str()) ? opcd(0) : ersIO_ERROR;
#endif
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::delete_directory(zstring src, bool recursive)
{
    opcd was_err = NOERR;

    if (recursive) {
        list_file_paths(src, "*", recursion_mode::recursive_dirs_exit, [&was_err](const charstr& path, list_entry type) {
            opcd err = type != list_entry::file
                ? delete_directory(path, false)
                : delete_file(path);

            if (was_err == NOERR && err != NOERR)
                was_err = err;
        });

        if (was_err != NOERR)
            return was_err;
    }

#ifdef SYSTYPE_MSVC
    return 0 == _rmdir(no_trail_sep(src)) ? opcd(0) : ersIO_ERROR;
#else
    return 0 == rmdir(no_trail_sep(src)) ? opcd(0) : ersIO_ERROR;
#endif
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::copymove_directory(zstring src, zstring dst, bool move)
{
    opcd was_err, err;
    uints slen = src.len();

    bool sdir = directory::is_separator(src.get_token().last_char());
    if (!sdir)
        ++slen;

    bool ddir = directory::is_separator(dst.get_token().last_char());

    //< src/ to dst or dst/ - move/copy content of src/ into dst/
    //< src to dst/ - move/copy src dir into dst/src
    //< src to dst  - rename src to dst

    charstr& dsts = dst.get_str();

    if (directory::is_valid_file(src)) {
        if (ddir) {
            //copy to dst/
            token file = src.get_token().cut_right_group_back("\\/"_T);
            dsts << file;
            err = move ? move_file(src, dsts, false) : copy_file(src, dsts, true);
        }
        else
            err = move ? move_file(src, dst, false) : copy_file(src, dst, true);

        return err;
    }

    if (!sdir) {
        if (ddir) {
            token folder = src.get_token().cut_right_group_back("\\/"_T);
            dsts << folder;
        }
        
        if (!is_valid_directory(dsts))
        {
            mkdir(dsts);
        }

        dsts << '/';
    }
    else if (!ddir)
        dsts << '/';

    uint dlen = dsts.len();

    list_file_paths(src, "*", recursion_mode::recursive_dirs_enter_exit, [&](const charstr& path, list_entry isdir) {
        token newpath = token(path.ptr() + slen, path.ptre());

        dsts.resize(dlen);
        dsts << newpath;

        if (isdir == list_entry::dir_enter) {
            err = mkdir(dsts);
        }
        else if (isdir == list_entry::dir_exit) {
            if (move)
                err = delete_directory(path, false);
        }
        else if (move)
            err = move_file(path, dsts, false);
        else
            err = copy_file(path, dsts, true);

        if (was_err == NOERR && err != NOERR)
            was_err = err;
    });

    if (was_err == NOERR && !sdir && move)
        was_err = delete_directory(src, false);

    return was_err;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::is_writable(zstring fname)
{
#ifdef SYSTYPE_MSVC
    return 0 == _access(no_trail_sep(fname), 2);
#else
    return 0 == access(no_trail_sep(fname), 2);
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool directory::set_writable(zstring fname, bool writable)
{
#ifdef SYSTYPE_MSVC
    return 0 == _chmod(no_trail_sep(fname), writable ? (S_IREAD | S_IWRITE) : S_IREAD);
#else
    return 0 == chmod(no_trail_sep(fname), writable ? (S_IREAD | S_IWRITE) : S_IREAD);
#endif
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::delete_files(token path_and_pattern)
{
    directory dir;
    opcd e = dir.open(path_and_pattern);
    if (e != NOERR) return e;

    while (dir.next()) {
        if (dir.get_last_file_name_token() == PARENT_DIR_SEGMENT)
            continue;

        opcd le = delete_file(dir.get_last_full_path());
        if (le != NOERR)
            e = le;
    }

    return e;
}

////////////////////////////////////////////////////////////////////////////////
opcd directory::mkdir_tree(token name, bool last_is_file, uint mode)
{
    bool dirend = false;
    while (name.last_char() == '/' || name.last_char() == '\\') {
        name.shift_end(-1);
        dirend = true;
    }

    zstring path = name;
    char* pc = (char*)path.c_str();

    for (uint i = 0; i < name.len(); ++i)
    {
        if (name[i] == '/' || name[i] == '\\')
        {
            char c = pc[i];
            pc[i] = 0;

            opcd e = mkdir(pc, mode);
            pc[i] = c;

            if (e != NOERR)  return e;
        }
    }

    return last_is_file && !dirend ? ersNOERR : mkdir(pc, mode);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::get_relative_path(token src, token dst, charstr& relout, bool last_src_is_file)
{
#ifdef SYSTYPE_WIN
    bool sf = src.nth_char(1) == ':';
    bool df = dst.nth_char(1) == ':';
#else
    bool sf = src.first_char() == '/';
    bool df = dst.first_char() == '/';
#endif

    if (sf != df) return false;

    if (sf) {
#ifndef SYSTYPE_WIN
        src.shift_start(1);
        dst.shift_start(1);
#endif
    }

    if (directory::is_separator(src.last_char()))
        src.shift_end(-1);

    const char* ps = src.ptr();
    const char* pe = 0;

    while (src.is_set() || dst.is_set())
    {
        token st = src.cut_left_group(DIR_SEPARATORS);
        token dt = dst.cut_left_group(DIR_SEPARATORS);

        bool isfile = last_src_is_file && src.is_empty();

#ifdef SYSTYPE_WIN
        if (isfile || !st.cmpeqi(dt)) {
#else
        if (isfile || st != dt) {
#endif
            src.set(st.ptr(), src.ptre());
            dst.set(dt.ptr(), dst.ptre());
            break;
        }

        pe = st.ptre();
    }

    token presrc = token(ps, pe);
    relout.reset();

    while (src) {
        token rt = src.cut_left_group(DIR_SEPARATORS);

        if (rt == PARENT_DIR_SEGMENT) {
            if (relout) {
                token r2 = relout;
                r2--;
                r2.cut_right_back(separator());
                relout.resize(r2.len());
            }
            else if (presrc) {
                token r2 = presrc.cut_right_group_back(DIR_SEPARATORS);
                relout << r2;
                relout << separator();
            }
            else
                return false;
        }
        else if (src || !last_src_is_file) {
            relout << PARENT_DIR_SEGMENT;
            relout << separator();
        }
    }

    return append_path(relout, dst);
}

////////////////////////////////////////////////////////////////////////////////
bool directory::compact_path(charstr& dst, char use_separator)
{
    return build_path_internal(dst, ""_T, dst, true, true, use_separator);
}

////////////////////////////////////////////////////////////////////////////////
coid::charstr directory::create_compact_path(const coid::token& path, char to_sep)
{
    coid::charstr result = path;

    if (!compact_path(result, to_sep))
    {
        return "";
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::list_file_paths(const token& path, const token& extension, recursion_mode mode,
    const coid::function<void(const charstr&, list_entry)>& fn)
{
    directory dir;

    if (dir.open(path, "*") != ersNOERR)
        return false;

    bool all_files = extension.is_empty() || extension == '*';
    bool ext_with_dot = extension.first_char() == '.';

    while (dir.next())
    {
        if (dir.is_entry_regular())
        {
            if (mode == recursion_mode::immediate_dirs_only || mode == recursion_mode::recursive_dirs_only)
                continue;

            bool valid = all_files;
            if (!all_files) {
                token fname = dir.get_last_file_name_token();

                if (fname.ends_with_icase(extension) && (ext_with_dot || fname.nth_char(-1 - ints(extension.len())) == '.'))
                    valid = true;
            }

            if (valid)
                fn(dir.get_last_full_path(), list_entry::file);
        }
        else if (dir.is_entry_subdirectory())
        {
            if (mode == recursion_mode::immediate_files_and_dirs || mode == recursion_mode::immediate_dirs_only) {
                fn(dir.get_last_full_path(), list_entry::dir_enter);
            }
            else if (mode != recursion_mode::immediate_files)
            {
                if (mode == recursion_mode::recursive_dirs_enter || mode == recursion_mode::recursive_dirs_enter_exit)
                    fn(dir.get_last_full_path(), list_entry::dir_enter);

                directory::list_file_paths(dir.get_last_full_path(), extension, mode, fn);

                if (mode == recursion_mode::recursive_dirs_exit || mode == recursion_mode::recursive_dirs_enter_exit)
                    fn(dir.get_last_full_path(), list_entry::dir_exit);
            }
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
uint64 directory::calculate_directory_size(const coid::token& path)
{
    if (!coid::directory::is_valid_directory(path))
    {
        return 0;
    }

    uint64 result = 0;

    list_file_paths(path, "", coid::directory::recursion_mode::recursive_files, [&result](const coid::charstr& path, coid::directory::list_entry){
        result += coid::directory::file_size(path);
    });

    return result;
}

////////////////////////////////////////////////////////////////////////////////
coid::token directory::get_path_component(const coid::token& path, uint32& root_length, path_component_enum component, coid::token* remainder, bool* is_root_component)
{
    if (root_length == -1)
    {
        root_length = get_path_root_length_internal(path);
    }

    coid::token remainder_tmp;
    coid::token& remainder_ref = remainder ? *remainder : remainder_tmp;
    coid::token result;

    //the root is what the call that zeroes the root length has consumed, an empty component alone
    //does not tell it apart, the unix root being the only root that comes back empty
    const bool had_root = root_length != 0;

    const bool valid = get_path_component_internal(path, root_length, component, result, remainder_ref);
    if (is_root_component) *is_root_component = valid && had_root && root_length == 0;

    return result;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::get_path_component_internal( 
    const coid::token& path,
    uint32& root_length_in_out,
    path_component_enum component,
    coid::token& result,
    coid::token& remainder
)
{
    if (root_length_in_out > 0)
    {
#ifdef SYSTYPE_WIN
        DASSERTX(root_length_in_out > 1, "This can be possible only in UNIX");
#endif // SYSTYPE_WIN

        if (path.len() <= root_length_in_out || component == path_component_enum::first)
        {
#ifdef SYSTYPE_WIN
            //the root is taken without the separator run behind it, but with the two leading ones of
            //a unc path, which are a part of it. The length is clamped because the walk reaches the
            //bare root with a path shorter than the root length ("C:" against a root length of 3)
            const uints root_len = path.len() < root_length_in_out ? path.len() : uints(root_length_in_out);
            result = coid::token(path.ptr(), root_len);
            while (!result.is_empty() && is_separator(result.last_char()))
                result.shift_end(-1);

            DASSERTX(result.is_set(), "Tha path is invalid. This can happen only in UNIX");
#else
            result = coid::token();
#endif // SYSTYPE_WIN
            remainder = coid::token(path.ptr() + root_length_in_out, path.ptre());
            root_length_in_out = 0;
            return true;
        }
    }

    remainder = path;
    if (component == path_component_enum::first)
    {
        result = remainder.cut_left_group(separators());
    }
    else
    {
        //a trailing separator run is not a component of its own
        if (is_separator(remainder.last_char()))
        {
            remainder.cut_right_group_back(separators());
        }

        result = remainder.cut_right_group_back(separators());
    }

    return result.is_set();
}

////////////////////////////////////////////////////////////////////////////////
bool directory::do_append_compact_internal(token path, charstr& result_in_out, bool normalize_separators_only, char use_separator, bool is_result_absolute, uint32& result_regular_component_count_in_out)
{
    token component;
    char next_separator = 0;
    const bool ends_with_separator = path.ends_with_any_of(DIR_SEPARATORS);

    uint32 root_length = 0;

    if (result_in_out.is_set() && !coid::token(result_in_out).ends_with_any_of(DIR_SEPARATORS))
    {
        next_separator = use_separator ? use_separator : separator();
    }

    while (path.is_set())
    {
        component = get_path_component(path, root_length, path_component_enum::first, &path);

        if (normalize_separators_only)
        {
            if (next_separator)
            {
                result_in_out << next_separator;
            }

            result_in_out << component;
        }
        else
        {
            if (component == PARENT_DIR_SEGMENT)
            {
                if (result_regular_component_count_in_out == 0)
                {
                    if (is_result_absolute) // Can't go below the root
                    {
                        return false;
                    }
                    else // Add the PARENT_DIR_SEGMENT to result
                    {
                        if (next_separator)
                        {
                            result_in_out << next_separator;
                        }
                        result_in_out << PARENT_DIR_SEGMENT;
                    }
                }
                else // Cut off last component from result
                {
                    token cutoff = get_path_component(result_in_out, root_length, path_component_enum::last);
                    if (cutoff.len() == result_in_out.len() - 1)
                    {
                        result_in_out.resize(0);
                    }
                    else
                    {
                        result_in_out.resize(-int(cutoff.len()) - 1);
                    }

                    --result_regular_component_count_in_out;
                }
            }
            else if (component != CURRENT_DIR_SEGMENT) // regular component. Add it to the result to result
            {
                if (next_separator)
                {
                    result_in_out << next_separator;
                }

                result_in_out << component;
                ++result_regular_component_count_in_out;
            }
        }

        // result is empty next component will be appended without leading separator
        if (result_in_out.is_empty())
        {
            next_separator = 0;
        }
        else if(next_separator != 0 || component != CURRENT_DIR_SEGMENT)  // when next separator is 0 and the component was current dir
        {
            // take the last separator from remaining path
            next_separator = use_separator ? use_separator : *(path.ptr() - 1);
        }
    }

    // treat original trailing separator
    if (ends_with_separator)
    {
        result_in_out << next_separator;
    }

    return true;
};


bool directory::build_path_internal(const token& base_path, const token& appended_path, charstr& result, bool make_compact, bool keep_below, char use_separator)
{
    charstr tmp_result(STACK_STRING(base_path.len() + appended_path.len() + 2)); // +1 zero termination +1 possible missing trailing separator in base_path
    const bool effective_compact_path = make_compact || keep_below;
    const bool normalize_separators_only = !effective_compact_path && use_separator != 0;
    const bool appended_path_is_absolute = is_absolute_path(appended_path);

    bool base_path_is_absolute = false;
    uint32 base_path_regular_component_count = 0;

    auto do_compact = [](const coid::token path, charstr& result, bool normalize_separators_only, char use_separator, bool* is_absolute = nullptr, uint32* regular_component_count = nullptr)->bool
    {
        bool is_root_tmp;
        bool& is_root_ref = is_absolute ? *is_absolute : is_root_tmp;
        uint32 root_len = -1;
        coid::token rel_path = path;
        coid::token root_component;
        // get the first compomonent that is not '.'
        do
        {
            root_component = get_path_component(rel_path, root_len, path_component_enum::first, &rel_path, &is_root_ref);
        } while(root_component == CURRENT_DIR_SEGMENT);

        result = root_component;

        // there must have been separator after the root
        if (root_component.ptre() != rel_path.ptre())
        {
            char separator = use_separator ? use_separator : *(rel_path.ptr() - 1);
            // malformed root (e.g. C:a)
            if (!is_separator(separator))
            {
                return false;
            }
            result << separator;
        }

        uint32 regular_component_count_tmp;
        uint32& regular_component_count_ref = regular_component_count ? *regular_component_count : regular_component_count_tmp;
        regular_component_count_ref = is_root_ref ? 0 : 1;

        return do_append_compact_internal(rel_path, result, normalize_separators_only, use_separator, is_root_ref, regular_component_count_ref);
    };

    if (base_path.is_set() && (effective_compact_path || normalize_separators_only))
    {
        if (!do_compact(base_path, tmp_result, normalize_separators_only, use_separator, &base_path_is_absolute, &base_path_regular_component_count))
        {
            return false;
        }
    }
    else 
    {
        tmp_result = base_path;
    }

    /// only compact beforehand when absolute
    if (appended_path.is_set())
    {
        if(appended_path_is_absolute)
        {
            if (!keep_below)
            {
                tmp_result = appended_path;
            }
            else 
            {
                charstr compact_appended_path(STACK_STRING(appended_path.len() + 1));
                if (!do_compact(appended_path, compact_appended_path, normalize_separators_only, use_separator))
                {
                    return false;
                }

                if (is_subpath(tmp_result, compact_appended_path))
                {
                    tmp_result.swap(compact_appended_path);
                }
                else
                {
                    return false;
                }
            }
        }
        else 
        {
            charstr compact_base_path;
            if (keep_below)
            {
                compact_base_path = charstr(STACK_STRING(tmp_result.len() + 1));
                compact_base_path.set_from(tmp_result);
            }

            if (effective_compact_path)
            {
                if (!do_append_compact_internal(appended_path, tmp_result, normalize_separators_only, use_separator, base_path_is_absolute, base_path_regular_component_count) || (keep_below && !is_subpath(compact_base_path, tmp_result)))
                {
                    return false;
                }
            }
            else 
            {
                if (tmp_result.is_set() && !tmp_result.get_token().ends_with_any_of(DIR_SEPARATORS))
                {
                    tmp_result << (use_separator ? use_separator : separator());
                }

                tmp_result << appended_path;
            }
        }
    }

    result = tmp_result;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
uint32 directory::get_path_root_length_internal(const coid::token& path)
{
    const uint path_len = path.len();
#ifdef SYSTYPE_WIN
    const bool is_dos_drive = path_len >= 2 && path[1] == ':';
    
    if (is_dos_drive)
    {
        return path.shifted_start(2).count_ingroup(separators()) + 2;
    }
    else 
    {
        const uint unc_root_len = get_unc_root_len(path);
        return unc_root_len > 0 ? (unc_root_len + path.shifted_start(unc_root_len).count_ingroup(separators())) : 0;
    }


#else
    return path.count_ingroup(separators());
#endif
}
COID_NAMESPACE_END
