#include <comm/dir.h>
#include <comm/commassert.h>
#include <comm/binstream/filestream.h>


void test_is_same_path()
{
    // --- Identical paths ---
    DASSERTX(coid::directory::is_same_path("C:\\foo\\bar", "C:\\foo\\bar"),
        "Identical absolute paths");

    DASSERTX(coid::directory::is_same_path("foo\\bar", "foo\\bar"),
        "Identical relative paths");

    // --- Trailing slashes ---
    DASSERTX(coid::directory::is_same_path("C:\\foo\\bar\\", "C:\\foo\\bar"),
        "Trailing backslash vs none");

    DASSERTX(coid::directory::is_same_path("C:\\foo\\bar/", "C:\\foo\\bar"),
        "Trailing forward slash vs none");

    // --- Slash style ---
    DASSERTX(coid::directory::is_same_path("C:\\foo\\bar", "C:/foo/bar"),
        "Backslash vs forward slash");

    DASSERTX(coid::directory::is_same_path("C:/foo\\bar", "C:\\foo/bar"),
        "Mixed slashes");

    DASSERTX(coid::directory::is_same_path("C:/foo\\bar", "C:\\foo//bar"),
        "Double slashes");

    // --- Case insensitivity ---
    DASSERTX(coid::directory::is_same_path("C:\\Foo\\Bar", "C:\\foo\\bar"),
        "Different cases");

    DASSERTX(coid::directory::is_same_path("C:\\foo", "c:\\foo"),
        "Drive letter case");

    // --- Dot segments ---
    DASSERTX(coid::directory::is_same_path("C:\\foo\\.\\bar", "C:\\foo\\bar"),
        "Dot in path");

    DASSERTX(coid::directory::is_same_path("C:\\foo\\baz\\..\\bar", "C:\\foo\\bar"),
        "Double dot in path");

    DASSERTX(coid::directory::is_same_path("C:\\foo\\a\\b\\..\\..\\bar", "C:\\foo\\bar"),
        "Multiple double dots");

    DASSERTX(coid::directory::is_same_path("C:\\foo\\a\\b\\..\\..\\bar", "C:\\foo\\a\\..\\a\\b\\..\\..\\bar"),
        "Multiple double dots");

    // --- Different paths (must return false) ---
    DASSERTX(!coid::directory::is_same_path("C:\\foo\\bar", "C:\\foo\\baz"),
        "Clearly different paths");

    DASSERTX(!coid::directory::is_same_path("C:\\foo\\bar", "D:\\foo\\bar"),
        "Different drives");

    DASSERTX(!coid::directory::is_same_path("C:\\foo\\bar", "C:\\foo"),
        "Subpath is not same");

    DASSERTX(!coid::directory::is_same_path("C:\\foo\\bar", "C:\\bar\\foo"),
        "Swapped segments");

    // --- Edge cases ---
    DASSERTX(coid::directory::is_same_path("", ""),
        "Empty vs empty");

    DASSERTX(!coid::directory::is_same_path("", "C:\\foo"),
        "Empty vs non-empty");

    DASSERTX(coid::directory::is_same_path("C:\\", "C:\\"),
        "Root path");

    DASSERTX(coid::directory::is_same_path("C:\\", "C:/"),
        "Root with different slash");

    DASSERTX(coid::directory::is_same_path("\\\\server\\share\\foo", "\\\\server\\share\\foo"),
        "UNC paths equal");

    DASSERTX(!coid::directory::is_same_path("\\\\server\\share\\foo", "\\\\server\\share\\bar"),
        "UNC paths different");
}

////////////////////////////////////////////////////////////////////////////////
///Creates a file with the given content at path (overwrites if it already exists)
static void create_test_file(const coid::token& path, const coid::token& content = "test_content"_T)
{
    coid::bofstream f(path);
    coid::uints len = content.len();
    f.write_raw(content.ptr(), len);
}

void test_directory_delete()
{
    coid::charstr root = coid::directory::create_tmp_dir("comm_test_delete_"_T);
    DASSERTX(!root.is_empty(), "Failed to create temporary root directory for delete tests");

    // --- Deleting an empty directory (non-recursive) should succeed ---
    {
        coid::charstr dir = root;
        dir << "/empty_dir";
        DASSERTX(coid::directory::mkdir(dir) == NOERR, "Failed to create empty_dir");

        DASSERTX(coid::directory::delete_directory(dir, false) == NOERR,
            "Non-recursive delete of an empty directory should succeed");
        DASSERTX(!coid::directory::is_valid_directory(dir),
            "Directory should no longer exist after deletion");
    }

    // --- Non-recursive delete of a directory containing a file must fail, and leave it intact ---
    {
        coid::charstr dir = root;
        dir << "/nonempty_dir";
        DASSERTX(coid::directory::mkdir(dir) == NOERR, "Failed to create nonempty_dir");

        coid::charstr file = dir;
        file << "/file.txt";
        create_test_file(file);

        DASSERTX(coid::directory::delete_directory(dir, false) != NOERR,
            "Non-recursive delete of a non-empty directory should fail");
        DASSERTX(coid::directory::is_valid_directory(dir),
            "Directory should still exist after a failed non-recursive delete");
        DASSERTX(coid::directory::is_valid_file(file),
            "File inside the directory should be untouched after a failed non-recursive delete");

        DASSERTX(coid::directory::delete_directory(dir, true) == NOERR,
            "Cleanup: recursive delete should succeed");
    }

    // --- Recursive delete removes files, nested subdirectories and the directory itself ---
    {
        coid::charstr dir = root;
        dir << "/recursive_dir";
        DASSERTX(coid::directory::mkdir(dir) == NOERR, "Failed to create recursive_dir");

        coid::charstr file1 = dir;
        file1 << "/a.txt";
        create_test_file(file1);

        coid::charstr nested = dir;
        nested << "/nested";
        DASSERTX(coid::directory::mkdir(nested) == NOERR, "Failed to create nested subdir");

        coid::charstr file2 = nested;
        file2 << "/b.txt";
        create_test_file(file2);

        DASSERTX(coid::directory::delete_directory(dir, true) == NOERR,
            "Recursive delete of a directory with files and subdirectories should succeed");
        DASSERTX(!coid::directory::is_valid_directory(dir),
            "Directory should no longer exist after a recursive delete");
    }

    // --- Deleting a directory that doesn't exist should fail, regardless of the recursive flag ---
    {
        coid::charstr missing = root;
        missing << "/does_not_exist";

        DASSERTX(coid::directory::delete_directory(missing, false) != NOERR,
            "Non-recursive delete of a non-existent directory should fail");
        DASSERTX(coid::directory::delete_directory(missing, true) != NOERR,
            "Recursive delete of a non-existent directory should fail");
    }

    // --- Cleanup ---
    DASSERTX(coid::directory::delete_directory(root, true) == NOERR,
        "Cleanup: failed to remove temporary root directory for delete tests");
}

void test_directory_move()
{
    coid::charstr root = coid::directory::create_tmp_dir("comm_test_move_"_T);
    DASSERTX(!root.is_empty(), "Failed to create temporary root directory for move tests");

    // --- move_to: source and destination both exist and don't collide -> source directory
    // ends up nested inside destination, at destination/<source_name>/, with its content intact ---
    {
        coid::charstr src = root;
        src << "/move_to_src";
        DASSERTX(coid::directory::mkdir(src) == NOERR, "Failed to create move_to_src");

        coid::charstr file = src;
        file << "/file.txt";
        create_test_file(file, "hello"_T);

        coid::charstr dst = root;
        dst << "/move_to_dst";
        DASSERTX(coid::directory::mkdir(dst) == NOERR, "Failed to create move_to_dst");

        coid::opcd e = coid::directory::move_directory(src, dst, coid::directory::move_directory_mode_enum::move_to);
        DASSERTX(e == NOERR, "move_to should succeed when destination doesn't already contain source's directory");

        coid::charstr result_dir = dst;
        result_dir << "/move_to_src";
        DASSERTX(coid::directory::is_valid_directory(result_dir),
            "Result directory destination/<source_name> should exist after move_to");
        DASSERTX(!coid::directory::is_valid_directory(src),
            "Source directory should no longer exist after move_to");

        coid::charstr result_file = result_dir;
        result_file << "/file.txt";
        DASSERTX(coid::directory::is_valid_file(result_file),
            "Moved file should exist at the new location");
        DASSERTX(coid::directory::file_size(result_file) == 5,
            "Moved file content should be preserved (size matches)");

        DASSERTX(coid::directory::delete_directory(dst, true) == NOERR, "Cleanup failed");
    }

    // --- move_to: nested subdirectories are moved recursively, preserving structure ---
    {
        coid::charstr src = root;
        src << "/nested_src";
        DASSERTX(coid::directory::mkdir(src) == NOERR, "Failed to create nested_src");

        coid::charstr inner = src;
        inner << "/inner";
        DASSERTX(coid::directory::mkdir(inner) == NOERR, "Failed to create nested_src/inner");

        coid::charstr file = inner;
        file << "/leaf.txt";
        create_test_file(file, "leaf"_T);

        coid::charstr dst = root;
        dst << "/nested_dst";
        DASSERTX(coid::directory::mkdir(dst) == NOERR, "Failed to create nested_dst");

        coid::opcd e = coid::directory::move_directory(src, dst, coid::directory::move_directory_mode_enum::move_to);
        DASSERTX(e == NOERR, "move_to should succeed for nested directory structures");

        coid::charstr result_file = dst;
        result_file << "/nested_src/inner/leaf.txt";
        DASSERTX(coid::directory::is_valid_file(result_file),
            "Nested file should be moved along with its parent directory structure");
        DASSERTX(!coid::directory::is_valid_directory(src),
            "Source directory tree should be fully removed after move_to");

        DASSERTX(coid::directory::delete_directory(dst, true) == NOERR, "Cleanup failed");
    }

    // --- move_to: fails when the source directory doesn't exist, destination is untouched ---
    {
        coid::charstr src = root;
        src << "/does_not_exist_src";

        coid::charstr dst = root;
        dst << "/move_to_dst_a";
        DASSERTX(coid::directory::mkdir(dst) == NOERR, "Failed to create move_to_dst_a");

        coid::opcd e = coid::directory::move_directory(src, dst, coid::directory::move_directory_mode_enum::move_to);
        DASSERTX(e != NOERR, "move_to should fail when the source directory doesn't exist");
        DASSERTX(coid::directory::is_valid_directory(dst),
            "Destination directory should remain untouched after a failed move_to");

        DASSERTX(coid::directory::delete_directory(dst, true) == NOERR, "Cleanup failed");
    }

    // --- move_to: fails when the destination directory doesn't exist, source is untouched ---
    {
        coid::charstr src = root;
        src << "/move_to_src_b";
        DASSERTX(coid::directory::mkdir(src) == NOERR, "Failed to create move_to_src_b");

        coid::charstr dst = root;
        dst << "/does_not_exist_dst";

        coid::opcd e = coid::directory::move_directory(src, dst, coid::directory::move_directory_mode_enum::move_to);
        DASSERTX(e != NOERR, "move_to should fail when the destination directory doesn't exist");
        DASSERTX(coid::directory::is_valid_directory(src),
            "Source directory should remain untouched after a failed move_to");

        DASSERTX(coid::directory::delete_directory(src, true) == NOERR, "Cleanup failed");
    }

    // --- move_to: fails when destination already contains a directory named like the source ---
    {
        coid::charstr src = root;
        src << "/collide";
        DASSERTX(coid::directory::mkdir(src) == NOERR, "Failed to create collide src");

        coid::charstr dst = root;
        dst << "/move_to_dst_c";
        DASSERTX(coid::directory::mkdir(dst) == NOERR, "Failed to create move_to_dst_c");

        coid::charstr existing = dst;
        existing << "/collide";
        DASSERTX(coid::directory::mkdir(existing) == NOERR,
            "Failed to create pre-existing colliding directory in destination");

        coid::opcd e = coid::directory::move_directory(src, dst, coid::directory::move_directory_mode_enum::move_to);
        DASSERTX(e != NOERR,
            "move_to should fail when destination already contains a directory with source's name");
        DASSERTX(coid::directory::is_valid_directory(src),
            "Source directory should remain untouched after a failed move_to");
        DASSERTX(coid::directory::is_valid_directory(existing),
            "Pre-existing colliding directory should remain untouched after a failed move_to");

        DASSERTX(coid::directory::delete_directory(src, true) == NOERR, "Cleanup failed");
        DASSERTX(coid::directory::delete_directory(dst, true) == NOERR, "Cleanup failed");
    }

    // --- rename: destination path doesn't exist yet -> source is moved/renamed directly to it ---
    {
        coid::charstr src = root;
        src << "/rename_src";
        DASSERTX(coid::directory::mkdir(src) == NOERR, "Failed to create rename_src");

        coid::charstr file = src;
        file << "/data.bin";
        create_test_file(file, "payload"_T);

        coid::charstr dst = root;
        dst << "/rename_dst";

        coid::opcd e = coid::directory::move_directory(src, dst, coid::directory::move_directory_mode_enum::rename);
        DASSERTX(e == NOERR, "rename should succeed when the destination doesn't exist");

        DASSERTX(coid::directory::is_valid_directory(dst),
            "Destination directory should exist after rename");
        DASSERTX(!coid::directory::is_valid_directory(src),
            "Source directory should no longer exist after rename");

        coid::charstr result_file = dst;
        result_file << "/data.bin";
        DASSERTX(coid::directory::is_valid_file(result_file),
            "Moved file should exist at the renamed location");
        DASSERTX(coid::directory::file_size(result_file) == 7,
            "Moved file content should be preserved (size matches) after rename");

        DASSERTX(coid::directory::delete_directory(dst, true) == NOERR, "Cleanup failed");
    }

    // --- rename: fails when the source directory doesn't exist ---
    {
        coid::charstr src = root;
        src << "/does_not_exist_rename_src";

        coid::charstr dst = root;
        dst << "/rename_dst_a";

        coid::opcd e = coid::directory::move_directory(src, dst, coid::directory::move_directory_mode_enum::rename);
        DASSERTX(e != NOERR, "rename should fail when the source directory doesn't exist");
        DASSERTX(!coid::directory::is_valid_directory(dst),
            "Destination should not be created when rename fails due to a missing source");
    }

    // --- rename: fails when the destination directory already exists, source is untouched ---
    {
        coid::charstr src = root;
        src << "/rename_src_b";
        DASSERTX(coid::directory::mkdir(src) == NOERR, "Failed to create rename_src_b");

        coid::charstr dst = root;
        dst << "/rename_dst_b";
        DASSERTX(coid::directory::mkdir(dst) == NOERR, "Failed to create pre-existing rename_dst_b");

        coid::opcd e = coid::directory::move_directory(src, dst, coid::directory::move_directory_mode_enum::rename);
        DASSERTX(e != NOERR, "rename should fail when the destination directory already exists");
        DASSERTX(coid::directory::is_valid_directory(src),
            "Source directory should remain untouched after a failed rename");

        DASSERTX(coid::directory::delete_directory(src, true) == NOERR, "Cleanup failed");
        DASSERTX(coid::directory::delete_directory(dst, true) == NOERR, "Cleanup failed");
    }

    // --- Cleanup ---
    DASSERTX(coid::directory::delete_directory(root, true) == NOERR,
        "Cleanup: failed to remove temporary root directory for move tests");
}

void test_get_path_component()
{
    using coid::directory;

    // --- default is `last`; remainder keeps the trailing separator attached, per the documented contract ---
    {
        coid::token remainder;
        coid::token last = directory::get_path_component("foo/bar/baz.txt"_T, &remainder);
        DASSERTX(last == "baz.txt"_T, "Last component should be the file name");
        DASSERTX(remainder == "foo/bar/"_T, "Remainder for `last` should keep the trailing separator attached");
    }

    // --- `first`: remainder has its leading separator stripped ---
    {
        coid::token remainder;
        coid::token first = directory::get_path_component("foo/bar/baz.txt"_T, &remainder, directory::path_component_enum::first);
        DASSERTX(first == "foo"_T, "First component should be the leading segment");
        DASSERTX(remainder == "bar/baz.txt"_T, "Remainder for `first` should not have a leading separator");
    }

    // --- backslash is only a separator on Windows; elsewhere it's an ordinary filename character ---
#ifdef SYSTYPE_WIN
    {
        coid::token remainder;
        coid::token last = directory::get_path_component("foo\\bar\\baz.txt"_T, &remainder);
        DASSERTX(last == "baz.txt"_T, "Last component should be the file name (backslash separators)");
        DASSERTX(remainder == "foo\\bar\\"_T, "Remainder should keep the trailing backslash separator");
    }
#else
    {
        // no '/' present, so the whole string is a single component; backslash must not split it
        coid::token remainder;
        coid::token last = directory::get_path_component("foo\\bar\\baz.txt"_T, &remainder);
        DASSERTX(last == "foo\\bar\\baz.txt"_T, "Backslash should not act as a separator on non-Windows systems");
        DASSERTX(remainder.is_empty(), "No '/' present, so there is nothing to split off into a remainder");
    }
#endif

    // --- a path that already ends with a separator doesn't produce an empty last component ---
    {
        coid::token remainder;
        coid::token last = directory::get_path_component("foo/bar/"_T, &remainder);
        DASSERTX(last == "bar"_T, "Pre-existing trailing separator should not produce an empty last component");
        DASSERTX(remainder == "foo/"_T, "Remainder should still end with a single separator");
    }

    // --- a path with a single component and no separator: `last` returns it whole, remainder is empty ---
    {
        coid::token remainder;
        coid::token last = directory::get_path_component("baz.txt"_T, &remainder);
        DASSERTX(last == "baz.txt"_T, "Single-component path should be returned whole for `last`");
        DASSERTX(remainder.is_empty(), "Remainder should be empty when there is no separator to split on");
    }

    // --- remainder_out left null (default) should behave exactly as before, no crash ---
    {
        coid::token last = directory::get_path_component("foo/bar/baz.txt"_T);
        DASSERTX(last == "baz.txt"_T, "Default call without remainder_out should still return the last component");
    }

    // --- in-place update: passing &path as remainder_out is safe, per the documented note ---
    {
        coid::token p = "foo/bar/baz.txt"_T;
        coid::token last = directory::get_path_component(p, &p);
        DASSERTX(last == "baz.txt"_T, "In-place call should still return the correct last component");
        DASSERTX(p == "foo/bar/"_T, "In-place call should leave the aliased path holding the remainder");
    }

    {
        coid::token p = "foo/bar/baz.txt"_T;
        coid::token first = directory::get_path_component(p, &p, directory::path_component_enum::first);
        DASSERTX(first == "foo"_T, "In-place call should still return the correct first component");
        DASSERTX(p == "bar/baz.txt"_T, "In-place call should leave the aliased path holding the remainder");
    }

    // --- DOS drive paths only exist as a concept on Windows; elsewhere "C:" etc. are just
    // ordinary (if unusual) filenames with no special drive handling ---

    // "C:" happens to come out the same on both platforms: on Windows via the dedicated drive
    // branch, on other systems because with no '/' present the whole thing is one opaque component
    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("C:"_T, &remainder);
        DASSERTX(comp == "C:"_T, "Bare \"C:\" should come back whole on any platform");
        DASSERTX(remainder.is_empty(), "Remainder for a bare \"C:\" should be empty");
    }

#ifdef SYSTYPE_WIN
    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("C:\\"_T, &remainder);
        DASSERTX(comp == "C:"_T, "Drive path with trailing separator should return the drive token");
        DASSERTX(remainder.is_empty(), "Remainder for a drive-with-separator path should be empty");
    }

#else
    {
        // no dedicated drive handling: "C:\" has no '/' in it, so it's a single opaque component
        coid::token remainder;
        coid::token comp = directory::get_path_component("C:\\"_T, &remainder);
        DASSERTX(comp == "C:\\"_T, "\"C:\\\\\" should be treated as an ordinary filename on non-Windows systems");
        DASSERTX(remainder.is_empty(), "Remainder for \"C:\\\\\" should be empty on non-Windows systems");
    }
#endif

    // --- DOS drive paths also support in-place aliasing (regression test: remainder_out must not be
    // written before the drive token is captured, since it may point at the same object as `path`) ---
    {
        coid::token p = "C:"_T;
        coid::token comp = directory::get_path_component(p, &p);
        DASSERTX(comp == "C:"_T, "In-place bare drive path should still return the drive token");
        DASSERTX(p.is_empty(), "In-place bare drive path should leave the aliased remainder empty");
    }

#ifdef SYSTYPE_WIN
    {
        coid::token p = "C:\\"_T;
        coid::token comp = directory::get_path_component(p, &p);
        DASSERTX(comp == "C:"_T, "In-place drive-with-separator path should still return the drive token");
        DASSERTX(p.is_empty(), "In-place drive-with-separator path should leave the aliased remainder empty");
    }
#else
    {
        coid::token p = "C:\\"_T;
        coid::token comp = directory::get_path_component(p, &p);
        DASSERTX(comp == "C:\\"_T, "In-place \"C:\\\\\" should still come back whole on non-Windows systems");
        DASSERTX(p.is_empty(), "In-place \"C:\\\\\" should leave the aliased remainder empty on non-Windows systems");
    }
#endif

    // --- note: an empty path is an asserted precondition violation (see the DASSERTX guard at the
    // top of get_path_component); it's still not exercised via a plain call here to avoid tripping
    // the debug assertion during test runs, even though it wouldn't return, and would in fact still
    // produce a well-defined (empty component, empty remainder) result via the root-only fallback.

    // --- a single-character path is a valid one-component relative path, not a degenerate case ---
    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("a"_T, &remainder);
        DASSERTX(comp == "a"_T, "Single-character relative path should be returned whole");
        DASSERTX(remainder.is_empty(), "Remainder for a single-character path should be empty");
    }

    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("a"_T, &remainder, directory::path_component_enum::first);
        DASSERTX(comp == "a"_T, "Single-character relative path should be returned whole for `first` too");
        DASSERTX(remainder.is_empty(), "Remainder for a single-character path should be empty");
    }

    // --- "a/": a single-character relative path with a trailing separator. The 2-char length must
    // still be recognized as having a trailing separator (regression test for the is_last_sep floor) ---
    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("a/"_T, &remainder);
        DASSERTX(comp == "a"_T, "Trailing separator on a single-character path should not swallow the component");
        DASSERTX(remainder.is_empty(), "Remainder for \"a/\" should be empty, not the separator itself");
    }

    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("a/"_T, &remainder, directory::path_component_enum::first);
        DASSERTX(comp == "a"_T, "\"a/\" should yield \"a\" as the first component too");
        DASSERTX(remainder.is_empty(), "Remainder for \"a/\" (first) should be empty");
    }

    // --- same, with a backslash separator (Windows-only; backslash isn't a separator elsewhere) ---
#ifdef SYSTYPE_WIN
    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("a\\"_T, &remainder);
        DASSERTX(comp == "a"_T, "Trailing backslash on a single-character path should not swallow the component");
        DASSERTX(remainder.is_empty(), "Remainder for \"a\\\\\" should be empty");
    }
#else
    {
        // "a\" is a 2-char filename on non-Windows systems: no separator, nothing to split off
        coid::token remainder;
        coid::token comp = directory::get_path_component("a\\"_T, &remainder);
        DASSERTX(comp == "a\\"_T, "\"a\\\\\" should be treated as a single opaque filename on non-Windows systems");
        DASSERTX(remainder.is_empty(), "Remainder for \"a\\\\\" should be empty on non-Windows systems");
    }
#endif

    // --- the following exercise POSIX-style absolute paths rooted at a bare "/", with no drive
    // letter or UNC prefix. That's not a well-formed Windows path, so these are POSIX-only ---
#ifndef SYSTYPE_WIN
    // --- absolute path "/a": the leading separator stays attached to the remainder, exactly like
    // any other separator would (this already worked before the root-only fix below; locked in here) ---
    {
        coid::token remainder;
        coid::token last = directory::get_path_component("/a"_T, &remainder);
        DASSERTX(last == "a"_T, "basename(\"/a\") should be \"a\"");
        DASSERTX(remainder == "/"_T, "dirname(\"/a\") should be the root \"/\", not empty");
    }

    // --- root path "/": nothing to split off. By convention (matching POSIX dirname()/basename(),
    // which both return "/" for input "/"), the component and remainder are both "/", for either mode ---
    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("/"_T, &remainder);
        DASSERTX(comp == "/"_T, "get_path_component(\"/\") should return the root itself, not empty");
        DASSERTX(remainder == "/"_T, "Remainder for the root path should also be \"/\"");
    }

    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("/"_T, &remainder, directory::path_component_enum::first);
        DASSERTX(comp == "/"_T, "get_path_component(\"/\", first) should also return the root itself");
        DASSERTX(remainder == "/"_T, "Remainder for the root path (first) should also be \"/\"");
    }

    // --- root path also supports in-place aliasing ---
    {
        coid::token p = "/"_T;
        coid::token comp = directory::get_path_component(p, &p);
        DASSERTX(comp == "/"_T, "In-place root path should still return \"/\"");
        DASSERTX(p == "/"_T, "In-place root path should leave the aliased remainder as \"/\"");
    }

    // --- a run of separators with nothing else ("///") is also treated as a bare root, unchanged ---
    {
        coid::token remainder;
        coid::token comp = directory::get_path_component("///"_T, &remainder);
        DASSERTX(comp == "///"_T, "A path made up solely of separators should come back unchanged");
        DASSERTX(remainder == "///"_T, "Remainder for an all-separator path should also come back unchanged");
    }
#endif //SYSTYPE_WIN
}

void run_directory_tests()
{
    test_is_same_path();
    test_directory_delete();
    test_directory_move();
    test_get_path_component();
}
