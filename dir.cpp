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
charstr directory::create_tmp_dir(const token& prefix)
{
    charstr tmpdir = get_tmp_dir();

    directory::treat_trailing_separator(tmpdir, true);

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
    return path.nth_char(1) == ':' || path.begins_with("\\\\"_T);
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
int directory::append_path(charstr& dst, token path, bool keep_below)
{
    if (is_absolute_path(path))
    {
        dst = path;

        return keep_below && !is_subpath(dst, path) ? -1 : 1;
    }
    else
    {
        char sep = separator();

        token tdst = dst;
        if (directory::is_separator(tdst.last_char())) {
            sep = tdst.last_char();
            tdst--;
        }

        bool is_below = true;

        while (path.begins_with(".."_T))
        {
            if (keep_below)
                is_below = false;

            path.shift_start(2);
            char c = path.first_char();

            if (c != 0 && !is_separator(c))
                return 0;           //bad path, .. not followed by a separator

            if (tdst.is_empty())
                return 0;           //too many .. in path

            token cut = tdst.cut_right_group_back(DIR_SEPARATORS, token::cut_trait_keep_sep_with_returned_default_full());
            if (directory::is_separator(cut.first_char()))
                sep = cut.first_char();

            if (c == 0) {
                dst.resize(tdst.len());
                return is_below ? 1 : -1;
            }

            ++path;
        }

        if (keep_below) {
            //check if the appended path doesn't escape out
            int rdepth = 0;
            token rp = path;
            while (token v = rp.cut_left_group(DIR_SEPARATORS))
            {
                if (v == '.');
                else if (v == ".."_T)
                    rdepth--;
                else
                    rdepth++;

                if (rdepth < 0) {
                    is_below = false;
                    break;
                }
            }
        }

        dst.resize(tdst.len());

        if (dst && !is_separator(dst.last_char()))
            dst << sep;

        dst << path;

        return is_below ? 1 : -1;
    }
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
    opcd was_err;

    if (recursive) {
        list_file_paths(src, "*", recursion_mode::recursive_dirs_enter, [&was_err](const charstr& path, list_entry type) {
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

    // src/ to dst or dst/ - copy content of src into dst/
    // src to dst/ - move src dir into dst/src
    // src to dst  - rename src to dst

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
        else if (!is_valid(dsts))
            mkdir(dsts);

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
        if (dir.get_last_file_name_token() == ".."_T)
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

        if (rt == ".."_T) {
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
            relout << ".."_T;
            relout << separator();
        }
    }

    return append_path(relout, dst) != 0;
}

////////////////////////////////////////////////////////////////////////////////
bool directory::compact_path(charstr& dst, char tosep)
{
    token dtok = dst;

#ifdef SYSTYPE_WIN
    bool absp = false;

    if (dtok.begins_with("\\\\"_T)) {
        absp = true;
        dtok.shift_start(2);
    }
    else if (dtok.nth_char(1) == ':') {
        absp = true;
        char c2 = dtok.nth_char(2);

        if (c2 != '/' && c2 != '\\' && c2 != 0)
            return false;
        if (!c2)
            return true;
        if (tosep)
            dst[2] = tosep;
        dtok.shift_start(2);
    }
#else
    bool absp = dtok.first_char() == '/';
    if (absp) {
        if (tosep)
            dst[0] = tosep;
        dtok.shift_start(1);
    }
#endif

    token fwd, rem;
    fwd.set_empty(dtok.ptr());

    int nfwd = 0;

    do {
        token seg = dtok.cut_left_group(DIR_SEPARATORS);
        bool isup = seg == ".."_T;

        ints d = dtok.ptr() - seg.ptre();
        if (d > 1) {
            //remove extra path separators
            dst.del(int(seg.ptre() - dst.ptr()), uint(d - 1));
            dtok.shift_start(1 - d);
            dtok.shift_end(1 - d);
        }

        //normalize path separator
        if (d > 0 && tosep)
            *(char*)seg.ptre() = tosep;

        if (seg == '.') {
            int d = int(dtok.ptr() - seg.ptr());
            dst.del(int(seg.ptr() - dst.ptr()), d);
            dtok.shift_start(-d);
            dtok.shift_end(-d);
        }
        else if (!isup) {
            if (rem.len()) {
                int rlen = rem.len();
                dst.del(int(rem.ptr() - dst.ptr()), rlen);
                dtok.shift_start(-rlen);
                dtok.shift_end(-rlen);
                rem.set_empty();
            }

            //count forward going tokens
            ++nfwd;
            fwd._pte = dtok ? (dtok.ptr() - 1) : dtok.ptr();
        }
        else if (nfwd) {
            //remove one token from fwd
            fwd.cut_right_group_back(DIR_SEPARATORS);
            rem.set(fwd.ptre(), dtok.first_char() ? (dtok.ptr() - 1) : dtok.ptr());
            --nfwd;
        }
        else {
            //no more forward tokens, remove rem range
            if (absp)
                return false;

            if (rem.len()) {
                if (rem.ptr() > dst.ptr())
                    rem.shift_start(-1);
                else if (rem.ptre() < dst.ptre())
                    rem.shift_end(1);

                int rlen = rem.len();

                dst.del(int(rem.ptr() - dst.ptr()), rlen);
                dtok.shift_start(-rlen);
                dtok.shift_end(-rlen);
                rem.set_empty();
            }

            fwd._ptr = dtok.ptr();
        }
    }
    while (dtok);

    if (rem.len())
        dst.del(int(rem.ptr() - dst.ptr()), rem.len());

    return true;
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

COID_NAMESPACE_END
