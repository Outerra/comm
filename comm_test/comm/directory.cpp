#include <comm/dir.h>
#include <comm/commassert.h>
#include <comm/binstream/filestream.h>


////////////////////////////////////////////////////////////////////////////////
///Tests of coid::directory, declared as a struct so the class can befriend it as a whole
///and the tests can reach the parts of it that are not public
struct directory_tests
{
    static void test_is_same_path();
    static void test_directory_delete();
    static void test_directory_move();
    static void test_get_path_root_length_internal();
    static void test_do_append_compact_internal();
    static void test_extract_path_component_internal();
    static void test_extract_path_component();
    static void test_extract_path_component_root();
    static void test_extract_path_component_unc();
    static void test_append_path();
    static void test_make_path();
    static void test_compact_path();
    static void test_create_compact_path();
    static void test_verify_path_syntax();
    static void test_build_path_internal();
};

////////////////////////////////////////////////////////////////////////////////


void directory_tests::test_is_same_path()
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

void directory_tests::test_directory_delete()
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

void directory_tests::test_directory_move()
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

void directory_tests::test_extract_path_component()
{
    using coid::directory;
    using component = coid::directory::path_component_enum;

    //a root length of -1 asks the call to measure the root, which is what an outside caller does.
    //The call keeps the variable up to date, so the same one drives the whole walk
    const uint32 measure = uint32(-1);

    // --- default is `last`, the separator between the component and the remainder is removed ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("foo/bar/baz.txt"_T, root_length, component::last, &remainder);
        DASSERTX(last == "baz.txt"_T, "Last component should be the file name");
        DASSERTX(remainder == "foo/bar"_T, "Remainder for `last` should come back without a trailing separator");
        DASSERTX(root_length == 0, "A relative path should measure no root");
    }

    // --- `first`: the remainder has its leading separator stripped ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token first = directory::extract_path_component("foo/bar/baz.txt"_T, root_length, component::first, &remainder);
        DASSERTX(first == "foo"_T, "First component should be the leading segment");
        DASSERTX(remainder == "bar/baz.txt"_T, "Remainder for `first` should not have a leading separator");
    }

    // --- backslash is only a separator on Windows; elsewhere it's an ordinary filename character ---
#ifdef SYSTYPE_WIN
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("foo\\bar\\baz.txt"_T, root_length, component::last, &remainder);
        DASSERTX(last == "baz.txt"_T, "Last component should be the file name (backslash separators)");
        DASSERTX(remainder == "foo\\bar"_T, "Remainder should come back without the trailing backslash separator");
    }
#else
    {
        // no '/' present, so the whole string is a single component; backslash must not split it
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("foo\\bar\\baz.txt"_T, root_length, component::last, &remainder);
        DASSERTX(last == "foo\\bar\\baz.txt"_T, "Backslash should not act as a separator on non-Windows systems");
        DASSERTX(remainder.is_empty(), "No '/' present, so there is nothing to split off into a remainder");
    }
#endif

    // --- a path that already ends with a separator doesn't produce an empty last component ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("foo/bar/"_T, root_length, component::last, &remainder);
        DASSERTX(last == "bar"_T, "Pre-existing trailing separator should not produce an empty last component");
        DASSERTX(remainder == "foo"_T, "Remainder should come back without a trailing separator");
    }

    // --- a path with a single component and no separator: `last` returns it whole, remainder is empty ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("baz.txt"_T, root_length, component::last, &remainder);
        DASSERTX(last == "baz.txt"_T, "Single-component path should be returned whole for `last`");
        DASSERTX(remainder.is_empty(), "Remainder should be empty when there is no separator to split on");
    }

    // --- remainder left null (default) should not fault ---
    {
        uint32 root_length = measure;
        coid::token last = directory::extract_path_component("foo/bar/baz.txt"_T, root_length);
        DASSERTX(last == "baz.txt"_T, "Default call without a remainder should still return the last component");
    }

    // --- in-place update: passing &path as the remainder is safe, per the documented note ---
    {
        uint32 root_length = measure;
        coid::token p = "foo/bar/baz.txt"_T;
        coid::token last = directory::extract_path_component(p, root_length, component::last, &p);
        DASSERTX(last == "baz.txt"_T, "In-place call should still return the correct last component");
        DASSERTX(p == "foo/bar"_T, "In-place call should leave the aliased path holding the remainder");
    }

    {
        uint32 root_length = measure;
        coid::token p = "foo/bar/baz.txt"_T;
        coid::token first = directory::extract_path_component(p, root_length, component::first, &p);
        DASSERTX(first == "foo"_T, "In-place call should still return the correct first component");
        DASSERTX(p == "bar/baz.txt"_T, "In-place call should leave the aliased path holding the remainder");
    }

    // --- a single-character path is a valid one-component relative path, not a degenerate case ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("a"_T, root_length, component::last, &remainder);
        DASSERTX(comp == "a"_T, "Single-character relative path should be returned whole");
        DASSERTX(remainder.is_empty(), "Remainder for a single-character path should be empty");
    }

    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("a/"_T, root_length, component::first, &remainder);
        DASSERTX(comp == "a"_T, "\"a/\" should yield \"a\" as the first component");
        DASSERTX(remainder.is_empty(), "Remainder for \"a/\" should be empty, not the separator itself");
    }

    // --- an exhausted path reports no component, which is what ends a walk ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component(""_T, root_length, component::last, &remainder);
        DASSERTX(comp.is_empty(), "An empty path has no component to hand back");
        DASSERTX(remainder.is_empty(), "An empty path leaves nothing over either");
    }

    // --- walking a relative path down, the measured root length carried along ---
    {
        uint32 root_length = measure;
        coid::token p = "a/b/c"_T;

        coid::token c = directory::extract_path_component(p, root_length, component::first, &p);
        DASSERTX(c == "a"_T, "The walk should start at the first component");
        DASSERTX(p == "b/c"_T, "The remainder should be the rest of the path");

        c = directory::extract_path_component(p, root_length, component::first, &p);
        DASSERTX(c == "b"_T, "The walk should continue with the next component");

        c = directory::extract_path_component(p, root_length, component::first, &p);
        DASSERTX(c == "c"_T, "The walk should end on the last component");
        DASSERTX(p.is_empty(), "The remainder should be empty once the last component is taken");
    }

    // --- DOS drive paths only exist as a concept on Windows; elsewhere "C:" is an ordinary
    // (if unusual) filename with no special drive handling ---
#ifdef SYSTYPE_WIN
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:"_T, root_length, component::last, &remainder);
        DASSERTX(comp == "C:"_T, "A bare drive is the root, and comes back whole");
        DASSERTX(remainder.is_empty(), "Remainder for a bare \"C:\" should be empty");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:\\"_T, root_length, component::last, &remainder);
        DASSERTX(comp == "C:"_T, "A drive with a trailing separator should return the drive token");
        DASSERTX(remainder.is_empty(), "Remainder for a drive-with-separator path should be empty");
    }

    // --- the drive is the first component of the path it heads ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:/a/b"_T, root_length, component::first, &remainder);
        DASSERTX(comp == "C:"_T, "The first component of a drive path is the drive");
        DASSERTX(remainder == "a/b"_T, "The remainder should hold the rest of the path");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    // --- `last` cuts below the drive, which stays at the head of the remainder ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:/a/b"_T, root_length, component::last, &remainder);
        DASSERTX(comp == "b"_T, "The last component is an ordinary one, cut below the drive");
        DASSERTX(remainder == "C:/a"_T, "The drive should stay on the remainder");
        DASSERTX(root_length == 3, "The root has not been reached, its length should be left alone");
    }

    // --- walking a drive path down reaches the drive at the end, in-place all the way ---
    {
        uint32 root_length = measure;
        coid::token p = "C:\\a"_T;

        coid::token comp = directory::extract_path_component(p, root_length, component::last, &p);
        DASSERTX(comp == "a"_T, "The component below the drive is an ordinary one");
        DASSERTX(p == "C:"_T, "The remainder should be the bare drive");

        comp = directory::extract_path_component(p, root_length, component::last, &p);
        DASSERTX(comp == "C:"_T, "Walking down should end at the drive");
        DASSERTX(p.is_empty(), "There should be nothing left once the drive is reached");
    }
#else
    {
        // no dedicated drive handling: "C:/a" is an ordinary relative path outside windows
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:/a"_T, root_length, component::last, &remainder);
        DASSERTX(root_length == 0, "A drive is not a root outside windows");
        DASSERTX(comp == "a"_T, "The last component should be peeled off as usual");
        DASSERTX(remainder == "C:"_T, "The drive should be an ordinary component of the remainder");
    }

    // --- the following exercise absolute paths rooted at a bare "/", which is not a well-formed
    // windows path, so they are POSIX-only ---

    // --- the unix root is a component of its own, and an empty one ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("/a/b"_T, root_length, component::first, &remainder);
        DASSERTX(comp.is_empty(), "The root of a path rooted at the volume root is an empty component");
        DASSERTX(remainder == "a/b"_T, "The remainder should hold the rest of the path");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    // --- a `last` component takes the separator in front of it along, the root with it ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("/a"_T, root_length, component::last, &remainder);
        DASSERTX(last == "a"_T, "basename(\"/a\") should be \"a\"");
        DASSERTX(remainder.is_empty(), "The separator run in front of the component goes with it, root and all");
    }

    // --- a path made up solely of separators is all root, and comes back as the empty component ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("/"_T, root_length, component::last, &remainder);
        DASSERTX(comp.is_empty(), "A bare root is an empty component");
        DASSERTX(remainder.is_empty(), "There is nothing below a bare root");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("///"_T, root_length, component::first, &remainder);
        DASSERTX(comp.is_empty(), "A run of separators is all root, and an empty component all the same");
        DASSERTX(remainder.is_empty(), "There is nothing below a bare root");
    }

    // --- a leading "//" is not a unc root outside windows, it is just the root separator run ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("//server/share/a"_T, root_length, component::last, &remainder);
        DASSERTX(comp == "a"_T, "The last component should be peeled off as usual");
        DASSERTX(remainder == "//server/share"_T, "The remainder should come back without a trailing separator");
    }
#endif //SYSTYPE_WIN
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_extract_path_component_root()
{
    using coid::directory;
    using component = coid::directory::path_component_enum;

    //the flag reports the call that consumed the root, which is the only way to tell the empty root
    //of a unix path apart from an ordinary component
    const uint32 measure = uint32(-1);

    // --- an ordinary component is not the root ---
    {
        uint32 root_length = measure;
        bool is_root = true;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("foo/bar/baz.txt"_T, root_length, component::first, &remainder, &is_root);
        DASSERTX(comp == "foo"_T, "The first component of a relative path is an ordinary one");
        DASSERTX(!is_root, "A relative path has no root component");
    }

    {
        uint32 root_length = measure;
        bool is_root = true;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("foo/bar/baz.txt"_T, root_length, component::last, &remainder, &is_root);
        DASSERTX(comp == "baz.txt"_T, "The last component of a relative path is an ordinary one");
        DASSERTX(!is_root, "A relative path has no root component");
    }

    // --- the flag is optional, passing null must not fault ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("foo/bar"_T, root_length, component::first, &remainder, nullptr);
        DASSERTX(comp == "foo"_T, "The flag should be optional");
    }

#ifdef SYSTYPE_WIN

    // --- a drive is the root component of the path it heads ---
    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:"_T, root_length, component::last, &remainder, &is_root);
        DASSERTX(comp == "C:"_T, "A bare drive comes back whole");
        DASSERTX(is_root, "A drive should be reported as the root component");
    }

    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:\\"_T, root_length, component::first, &remainder, &is_root);
        DASSERTX(comp == "C:"_T, "A drive root comes back as the drive");
        DASSERTX(is_root, "A drive should be reported as the root component");
    }

    // --- the drive heading a longer path is the root component just the same ---
    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:/a/b"_T, root_length, component::first, &remainder, &is_root);
        DASSERTX(comp == "C:"_T, "The first component of a drive path is the drive");
        DASSERTX(remainder == "a/b"_T, "The remainder should hold the rest of the path");
        DASSERTX(is_root, "A drive heading a longer path should be reported as the root component");
    }

    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:/../a"_T, root_length, component::first, &remainder, &is_root);
        DASSERTX(comp == "C:"_T, "The first component is the drive, whatever follows it");
        DASSERTX(remainder == "../a"_T, "The remainder should hold the rest of the path");
        DASSERTX(is_root, "A drive followed by a parent segment should still be the root component");
    }

    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:\\a\\b"_T, root_length, component::first, &remainder, &is_root);
        DASSERTX(comp == "C:"_T, "The separator style should not matter");
        DASSERTX(is_root, "A drive heading a longer path should be reported as the root component");
    }

    // --- the components below the drive are ordinary ones ---
    {
        uint32 root_length = measure;
        bool is_root = true;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("C:/../a"_T, root_length, component::last, &remainder, &is_root);
        DASSERTX(comp == "a"_T, "The last component is an ordinary one");
        DASSERTX(!is_root, "A component below the drive is not the root");
    }

    // --- walking a drive path down reaches the root only at the end ---
    {
        uint32 root_length = measure;
        bool is_root = true;
        coid::token p = "C:\\a"_T;

        coid::token comp = directory::extract_path_component(p, root_length, component::last, &p, &is_root);
        DASSERTX(comp == "a"_T, "The component below the drive is an ordinary one");
        DASSERTX(!is_root, "A component below the drive is not the root");

        comp = directory::extract_path_component(p, root_length, component::last, &p, &is_root);
        DASSERTX(comp == "C:"_T, "Walking down should end at the drive");
        DASSERTX(is_root, "The drive reached at the end is the root component");
    }

    // --- the unc root is reported the same way a drive is ---
    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("\\\\server\\share\\a"_T, root_length, component::first, &remainder, &is_root);
        DASSERTX(comp == "\\\\server"_T, "The first component of a unc path is its root");
        DASSERTX(is_root, "The unc root should be reported as the root component");
    }

    {
        uint32 root_length = measure;
        bool is_root = true;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("\\\\server\\share\\a"_T, root_length, component::last, &remainder, &is_root);
        DASSERTX(comp == "a"_T, "The last component of a unc path is an ordinary one");
        DASSERTX(!is_root, "A component below the unc root is not the root");
    }

    {
        uint32 root_length = measure;
        bool is_root = true;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("\\\\server\\share"_T, root_length, component::last, &remainder, &is_root);
        DASSERTX(comp == "share"_T, "The share is an ordinary component");
        DASSERTX(!is_root, "The share is not the root component");
    }

    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("\\\\server"_T, root_length, component::last, &remainder, &is_root);
        DASSERTX(comp == "\\\\server"_T, "A root only unc path comes back whole");
        DASSERTX(is_root, "The unc root should be reported as the root component");
    }

#else

    // --- a path starting at the volume root has the empty string for its root component ---
    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("/a/b"_T, root_length, component::first, &remainder, &is_root);
        DASSERTX(comp.is_empty(), "The root component of a path starting at the volume root is empty");
        DASSERTX(remainder == "a/b"_T, "The remainder should hold the rest of the path");
        DASSERTX(is_root, "An empty first component should be reported as the root component");
    }

    // --- a path made solely of separators is its own root ---
    {
        uint32 root_length = measure;
        bool is_root = false;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("/"_T, root_length, component::last, &remainder, &is_root);
        DASSERTX(comp.is_empty(), "A bare root is an empty component");
        DASSERTX(is_root, "A bare root should be reported as the root component");
    }

    // --- the flag is what tells that empty root apart from an ordinary component ---
    {
        uint32 root_length = measure;
        bool is_root = true;
        coid::token remainder;
        coid::token comp = directory::extract_path_component("a/b"_T, root_length, component::first, &remainder, &is_root);
        DASSERTX(comp == "a"_T, "A relative path starts with an ordinary component");
        DASSERTX(!is_root, "A relative path has no root component");
    }

#endif //SYSTYPE_WIN
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_extract_path_component_unc()
{
#ifdef SYSTYPE_WIN
    using coid::directory;
    using component = coid::directory::path_component_enum;

    const uint32 measure = uint32(-1);

    // --- "\\\\server" is the root component, the counterpart of the "C:" of a drive path ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token first = directory::extract_path_component("\\\\server\\share\\a\\b"_T, root_length, component::first, &remainder);
        DASSERTX(first == "\\\\server"_T, "The first component of a unc path should be its root");
        DASSERTX(remainder == "share\\a\\b"_T, "Remainder for `first` should not have a leading separator");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    // --- the share follows as an ordinary component of its own ---
    {
        uint32 root_length = measure;
        coid::token remainder = "share\\a\\b"_T;
        coid::token first = directory::extract_path_component(remainder, root_length, component::first, &remainder);
        DASSERTX(first == "share"_T, "The share should come back as a component of its own");
        DASSERTX(remainder == "a\\b"_T, "Remainder for `first` should not have a leading separator");
    }

    // --- both separator styles are recognized ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token first = directory::extract_path_component("//server/share/a/b"_T, root_length, component::first, &remainder);
        DASSERTX(first == "//server"_T, "A unc path written with forward slashes should be recognized too");
        DASSERTX(remainder == "share/a/b"_T, "Remainder for `first` should not have a leading separator");
    }

    // --- `last` cuts the components off the end as usual, the root staying on the remainder ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("\\\\server\\share\\a\\b"_T, root_length, component::last, &remainder);
        DASSERTX(last == "b"_T, "The last component should be cut off as usual");
        DASSERTX(remainder == "\\\\server\\share\\a"_T, "The remainder should come back without a trailing separator");
    }

    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("\\\\server\\share\\a"_T, root_length, component::last, &remainder);
        DASSERTX(last == "a"_T, "The only component below the share should be cut off");
        DASSERTX(remainder == "\\\\server\\share"_T, "The remainder should come back without a trailing separator");
    }

    // --- the share is cut like any other component, the root stays with the remainder ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("\\\\server\\share"_T, root_length, component::last, &remainder);
        DASSERTX(last == "share"_T, "The share should be cut off like any other component");
        DASSERTX(remainder == "\\\\server"_T, "The remainder should be the bare root the next call consumes");
    }

    // --- once the root is reached there is nothing left below it ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("\\\\server"_T, root_length, component::last, &remainder);
        DASSERTX(last == "\\\\server"_T, "The root should be the last component that can be cut off");
        DASSERTX(remainder.is_empty(), "Remainder for a root only path should be empty");
    }

    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token first = directory::extract_path_component("\\\\server"_T, root_length, component::first, &remainder);
        DASSERTX(first == "\\\\server"_T, "A root only path has the root as its only component");
        DASSERTX(remainder.is_empty(), "Remainder for a root only path should be empty");
    }

    // --- a trailing separator does not produce an empty component ---
    {
        uint32 root_length = measure;
        coid::token remainder;
        coid::token last = directory::extract_path_component("\\\\server\\share\\"_T, root_length, component::last, &remainder);
        DASSERTX(last == "share"_T, "A trailing separator should not produce an empty component");
        DASSERTX(remainder == "\\\\server"_T, "The remainder should be the bare root");
    }

    // --- the components concatenate back into the path they came from ---
    {
        uint32 root_length = measure;
        coid::token p = "\\\\server\\share\\a\\b"_T;
        coid::charstr rebuilt;

        rebuilt << directory::extract_path_component(p, root_length, component::first, &p);

        while (!p.is_empty())
            rebuilt << directory::separator() << directory::extract_path_component(p, root_length, component::first, &p);

        DASSERTX(coid::token(rebuilt) == "\\\\server\\share\\a\\b"_T, "The components should concatenate back into the original path");
    }

    // --- cutting the whole path down component by component ---
    {
        uint32 root_length = measure;
        coid::token p = "\\\\server\\share\\a\\b"_T;

        coid::token c = directory::extract_path_component(p, root_length, component::last, &p);
        DASSERTX(c == "b"_T, "In-place cutting should return the last component");

        c = directory::extract_path_component(p, root_length, component::last, &p);
        DASSERTX(c == "a"_T, "In-place cutting should continue with the next component");

        c = directory::extract_path_component(p, root_length, component::last, &p);
        DASSERTX(c == "share"_T, "In-place cutting should continue with the share");

        c = directory::extract_path_component(p, root_length, component::last, &p);
        DASSERTX(c == "\\\\server"_T, "In-place cutting should end with the root");
        DASSERTX(p.is_empty(), "The aliased remainder should be empty once the root is reached");
    }
#endif //SYSTYPE_WIN
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_do_append_compact_internal()
{
    using coid::directory;

    //with use_separator zero and no separator at the end of the result, the components are joined
    //with the platform one, the separators of the path itself being kept as they are written
    const char sep = directory::separator();

    // --- a plain component is joined with the platform separator ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("c"_T, result, false, 0, false, count);

        coid::charstr expected = "a/b";
        expected << sep << "c";

        DASSERTX(ok, "Appending a plain component should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The component should be joined with the platform separator");
        DASSERTX(count == 3, "The appended component should be counted");
    }

    // --- only the join is the platform one, the path keeps the separators it is written with ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("c/d/e"_T, result, false, 0, false, count);

        coid::charstr expected = "a/b";
        expected << sep << "c/d/e";

        DASSERTX(ok, "Appending a multi component path should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The separators of the path should be kept as they are");
        DASSERTX(count == 5, "Every appended component should be counted");
    }

    // --- a result that already ends with a separator does not get a second one ---
    {
        coid::charstr result = "a/";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("b"_T, result, false, 0, false, count);

        DASSERTX(ok, "Appending to a result ending with a separator should succeed");
        DASSERTX(coid::token(result) == "a/b"_T, "The separator already there should be the only one");
        DASSERTX(count == 2, "The appended component should be counted");
    }

    // --- a separator run in the path collapses into the single one it ends with ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("c//d"_T, result, false, 0, false, count);

        coid::charstr expected = "a/b";
        expected << sep << "c/d";

        DASSERTX(ok, "Appending across a separator run should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "A separator run should collapse into one");
        DASSERTX(count == 4, "A separator run should not produce an empty component");
    }

    // --- a trailing separator of the path is kept on the result ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("c/"_T, result, false, 0, false, count);

        coid::charstr expected = "a/b";
        expected << sep << "c/";

        DASSERTX(ok, "Appending a directory path should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The trailing separator should be kept");
        DASSERTX(count == 3, "A trailing separator is not a component of its own");
    }

    // --- a current dir segment resolves to nothing at all, and leaves no separator behind. The
    // component behind it carries the separator that sits in front of it, so the OS default one is
    // not the join here ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("./c"_T, result, false, 0, false, count);

        DASSERTX(ok, "Appending across a current dir segment should succeed");
        DASSERTX(coid::token(result) == "a/b/c"_T, "The component should be joined with the separator written in front of it");
        DASSERTX(count == 3, "A current dir segment is not a regular component");
    }

    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("c/."_T, result, false, 0, false, count);

        coid::charstr expected = "a/b";
        expected << sep << "c";

        DASSERTX(ok, "A trailing current dir segment should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "A trailing current dir segment should leave no separator behind");
    }

    // --- the same against a result that holds no separator at all: the OS default one is still not
    // the join, the component behind the current dir segment brings its own ---
    {
        coid::charstr result = "a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("./b"_T, result, false, 0, false, count);

        DASSERTX(ok, "Appending across a current dir segment should succeed");
        DASSERTX(coid::token(result) == "a/b"_T, "The component should be joined with the separator written in front of it");
        DASSERTX(count == 2, "A current dir segment is not a regular component");
    }

    // --- a parent dir segment cuts the last component off the result, what follows it is joined
    // with the separator it was written with in the path ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("../c"_T, result, false, 0, false, count);

        DASSERTX(ok, "Appending across a parent dir segment should succeed");
        DASSERTX(coid::token(result) == "a/c"_T, "The parent segment should consume the last component of the result");
        DASSERTX(count == 2, "The count should follow what the result holds");
    }

    {
        coid::charstr result = "a/b/c";
        uint32 count = 3;
        const bool ok = directory::do_append_compact_internal("../../d"_T, result, false, 0, false, count);

        DASSERTX(ok, "Two parent segments in a row should succeed");
        DASSERTX(coid::token(result) == "a/d"_T, "Each parent segment should consume one component");
        DASSERTX(count == 2, "The count should follow what the result holds");
    }

    // --- a parent segment resolving against a result that ends with a separator: the component
    // goes with the separator in front of it, the trailing one of the result staying where it is ---
    {
        coid::charstr result = "a/b/";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal(".."_T, result, false, 0, false, count);

        DASSERTX(ok, "A parent segment should succeed");
        DASSERTX(coid::token(result) == "a/"_T, "The result should keep the trailing separator it came with");
        DASSERTX(count == 1, "The count should follow what the result holds");
    }

    // --- a relative result may be resolved away entirely ---
    {
        coid::charstr result = "a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal(".."_T, result, false, 0, false, count);

        DASSERTX(ok, "Resolving a relative result away should succeed");
        DASSERTX(result.is_empty(), "The only component of the result should be gone");
        DASSERTX(count == 0, "There should be no regular component left");
    }

    // --- a component appended to an emptied result does not start with a separator ---
    {
        coid::charstr result = "a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("../b"_T, result, false, 0, false, count);

        DASSERTX(ok, "Resolving a relative result away and appending should succeed");
        DASSERTX(coid::token(result) == "b"_T, "The result should not start with a separator");
        DASSERTX(count == 1, "The appended component should be counted");
    }

    // --- a parent segment with nothing to resolve against is kept, a relative result having no
    // root to fail against ---
    {
        coid::charstr result = "a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("../.."_T, result, false, 0, false, count);

        DASSERTX(ok, "Climbing out of a relative result should succeed");
        DASSERTX(coid::token(result) == ".."_T, "The parent segment that resolves against nothing should be kept");
        DASSERTX(count == 0, "A parent segment is not a regular component");
    }

    {
        coid::charstr result = "a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("../../b"_T, result, false, 0, false, count);

        DASSERTX(ok, "Climbing out of a relative result should succeed");
        DASSERTX(coid::token(result) == "../b"_T, "The kept parent segment should be joined with what follows it");
        DASSERTX(count == 1, "Only the regular component should be counted");
    }

    // --- climbing above the root of an absolute result is what makes the call fail ---
    {
        coid::charstr result = "a";
        uint32 count = 0;
        const bool ok = directory::do_append_compact_internal(".."_T, result, false, 0, true, count);

        DASSERTX(!ok, "Climbing above the root of an absolute result should fail");
    }

    // --- use_separator is what the call joins with, the result is left as it is ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("c/d"_T, result, false, '\\', false, count);

        DASSERTX(ok, "Appending with a target separator should succeed");
        DASSERTX(coid::token(result) == "a/b\\c\\d"_T, "Only the separators the call writes should be the target one");
    }

    {
        coid::charstr result = "a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("b/"_T, result, false, '\\', false, count);

        DASSERTX(ok, "Appending with a target separator should succeed");
        DASSERTX(coid::token(result) == "a\\b\\"_T, "The trailing separator should be the target one as well");
    }

    // --- normalizing only, the dot segments are components like any other ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("./c//../d"_T, result, true, 0, false, count);

        coid::charstr expected = "a/b";
        expected << sep << "./c/../d";

        DASSERTX(ok, "Normalizing should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The dot segments should be kept and the runs collapsed");
        DASSERTX(count == 2, "Normalizing should not count components");
    }

    // --- normalizing only resolves nothing, a parent segment is left where it is instead of
    // consuming the last component of the result ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("../c"_T, result, true, 0, false, count);

        coid::charstr expected = "a/b";
        expected << sep << "../c";

        DASSERTX(ok, "Normalizing should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "Without compacting the parent segment should be left alone");
        DASSERTX(count == 2, "Normalizing should not count components");
    }

    // --- the path is taken by value, the caller's token comes back untouched ---
    {
        coid::charstr result = "a";
        uint32 count = 1;
        coid::token path = "b/c"_T;
        const bool ok = directory::do_append_compact_internal(path, result, false, 0, false, count);

        DASSERTX(ok, "Appending should succeed");
        DASSERTX(path == "b/c"_T, "The path should be left alone by the call");
    }

    // --- an empty path leaves the result as it is ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal(""_T, result, false, 0, false, count);

        DASSERTX(ok, "An empty path should succeed");
        DASSERTX(coid::token(result) == "a/b"_T, "An empty path should leave the result untouched");
        DASSERTX(count == 2, "An empty path should leave the count untouched");
    }

#ifdef SYSTYPE_WIN
    // --- the join is the platform separator whatever the result is written with ---
    {
        coid::charstr result = "a\\b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("c"_T, result, false, 0, false, count);

        DASSERTX(ok, "Appending to a backslash written result should succeed");
        DASSERTX(coid::token(result) == "a\\b\\c"_T, "The component should be joined with the platform separator");
    }

    // --- the separators of the path are kept, only the join is the platform one ---
    {
        coid::charstr result = "a/b";
        uint32 count = 2;
        const bool ok = directory::do_append_compact_internal("c\\d"_T, result, false, 0, false, count);

        DASSERTX(ok, "Appending a mixed style path should succeed");
        DASSERTX(coid::token(result) == "a/b\\c\\d"_T, "The path should keep the separator it is written with");
    }

    // --- the root of an absolute result is never cut into ---
    {
        coid::charstr result = "C:\\a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("..\\b"_T, result, false, 0, true, count);

        DASSERTX(ok, "Resolving down to the root should succeed");
        DASSERTX(coid::token(result) == "C:\\b"_T, "The drive should be left alone and joined with the component");
        DASSERTX(count == 1, "The count should follow what the result holds");
    }

    {
        coid::charstr result = "C:\\a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("..\\.."_T, result, false, 0, true, count);

        DASSERTX(!ok, "Climbing above the root of a drive path should fail");
    }
#else
    // --- the root of an absolute result is never cut into ---
    {
        coid::charstr result = "/a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("../b"_T, result, false, 0, true, count);

        DASSERTX(ok, "Resolving down to the root should succeed");
        DASSERTX(coid::token(result) == "/b"_T, "The root should be left alone and joined with the component");
        DASSERTX(count == 1, "The count should follow what the result holds");
    }

    {
        coid::charstr result = "/a";
        uint32 count = 1;
        const bool ok = directory::do_append_compact_internal("../.."_T, result, false, 0, true, count);

        DASSERTX(!ok, "Climbing above the root should fail");
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_get_path_root_length_internal()
{
    using coid::directory;

    // --- a relative path has no root, wherever its separators sit ---
    {
        DASSERTX(directory::get_path_root_length_internal(""_T) == 0, "An empty path measures no root");
        DASSERTX(directory::get_path_root_length_internal("a"_T) == 0, "A bare name has no root");
        DASSERTX(directory::get_path_root_length_internal("a/b"_T) == 0, "A relative path has no root");
        DASSERTX(directory::get_path_root_length_internal("a/b/"_T) == 0, "Only a leading separator run could be a root");
        DASSERTX(directory::get_path_root_length_internal("./a"_T) == 0, "A current dir segment is not a root");
        DASSERTX(directory::get_path_root_length_internal("../a"_T) == 0, "A parent segment is not a root");
    }

#ifdef SYSTYPE_WIN
    // --- a drive is a root, with the separator run behind it ---
    {
        DASSERTX(directory::get_path_root_length_internal("C:"_T) == 2, "A bare drive is all root");
        DASSERTX(directory::get_path_root_length_internal("C:\\"_T) == 3, "The separator behind the drive belongs to the root");
        DASSERTX(directory::get_path_root_length_internal("C:/"_T) == 3, "Both separator styles count");
        DASSERTX(directory::get_path_root_length_internal("C:\\\\"_T) == 4, "The whole run behind the drive belongs to the root");
        DASSERTX(directory::get_path_root_length_internal("C:\\a"_T) == 3, "The root ends where the path below it starts");
        DASSERTX(directory::get_path_root_length_internal("C:/a/b"_T) == 3, "Only the leading run belongs to the root");
        DASSERTX(directory::get_path_root_length_internal("c:/a"_T) == 3, "The case of the drive letter does not matter");
    }

    // --- a drive relative path carries the drive and nothing else ---
    {
        DASSERTX(directory::get_path_root_length_internal("C:a"_T) == 2, "A drive without a separator behind it is still the root");
    }

    // --- nothing is validated here, any second character ':' makes a drive ---
    {
        DASSERTX(directory::get_path_root_length_internal("1:\\a"_T) == 3, "The drive letter is not validated, that is verify_path_syntax' business");
    }

    // --- a windows path that starts with a single separator is drive relative, not rooted ---
    {
        DASSERTX(directory::get_path_root_length_internal("\\a\\b"_T) == 0, "A single leading separator is not a root on windows");
        DASSERTX(directory::get_path_root_length_internal("/a/b"_T) == 0, "A single leading separator is not a root on windows");
        DASSERTX(directory::get_path_root_length_internal("\\"_T) == 0, "A lone separator is not a root on windows");
    }

    // --- the server of a unc path is a root, with the separator run behind it ---
    {
        DASSERTX(directory::get_path_root_length_internal("\\\\server"_T) == 8, "A bare server is all root");
        DASSERTX(directory::get_path_root_length_internal("\\\\server\\"_T) == 9, "The separator behind the server belongs to the root");
        DASSERTX(directory::get_path_root_length_internal("\\\\server\\share"_T) == 9, "The share is an ordinary component below the root");
        DASSERTX(directory::get_path_root_length_internal("\\\\server\\share\\a"_T) == 9, "The root ends where the share starts");
        DASSERTX(directory::get_path_root_length_internal("//server/share/a"_T) == 9, "Both separator styles count");
        DASSERTX(directory::get_path_root_length_internal("\\\\server\\\\share"_T) == 10, "The whole run behind the server belongs to the root");
    }

    // --- a unc prefix carrying no server is not a path at all, and measures no root ---
    {
        DASSERTX(directory::get_path_root_length_internal("\\\\"_T) == 0, "A bare unc prefix measures no root");
        DASSERTX(directory::get_path_root_length_internal("\\\\\\share"_T) == 0, "A unc path with an empty server measures no root");
    }
#else
    // --- the leading separator run is the root, there being no drive nor unc path to speak of ---
    {
        DASSERTX(directory::get_path_root_length_internal("/"_T) == 1, "The root path is all root");
        DASSERTX(directory::get_path_root_length_internal("///"_T) == 3, "A run of separators is all root");
        DASSERTX(directory::get_path_root_length_internal("/a"_T) == 1, "The root ends where the path below it starts");
        DASSERTX(directory::get_path_root_length_internal("/a/b"_T) == 1, "Only the leading run belongs to the root");
        DASSERTX(directory::get_path_root_length_internal("//a"_T) == 2, "The whole leading run belongs to the root");
    }

    {
        DASSERTX(directory::get_path_root_length_internal("C:/a"_T) == 0, "A drive is an ordinary name outside windows");
        DASSERTX(directory::get_path_root_length_internal("\\a\\b"_T) == 0, "A backslash is an ordinary name character outside windows");
        DASSERTX(directory::get_path_root_length_internal("//server/share/a"_T) == 2, "A leading run is a root, not a unc prefix, outside windows");
    }
#endif

    // --- what is measured here is what extract_path_component_internal is fed, the part below the root
    // never starting with a separator ---
    {
        const coid::token path = "a/b"_T;
        uint32 root_length = directory::get_path_root_length_internal(path);

        coid::token result;
        coid::token remainder;
        const bool ok = directory::extract_path_component_internal(path, root_length, directory::path_component_enum::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "a"_T, "A relative path is cut whole");
        DASSERTX(remainder == "b"_T, "The remainder should hold the rest of the path");
        DASSERTX(root_length == 0, "A relative path should leave the root length at zero");
    }

#ifdef SYSTYPE_WIN
    {
        const coid::token path = "C:/a/b"_T;
        uint32 root_length = directory::get_path_root_length_internal(path);

        DASSERTX(!directory::is_separator(path.shifted_start(root_length).first_char()),
            "The part below the root should never start with a separator");

        coid::token result;
        coid::token remainder;
        const bool ok = directory::extract_path_component_internal(path, root_length, directory::path_component_enum::first, result, remainder);
        DASSERTX(ok, "Consuming the root should report a valid component");
        DASSERTX(result == "C:"_T, "The measured root should come back as the first component");
        DASSERTX(remainder == "a/b"_T, "The remainder should be the whole of the path below the root");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    // --- the whole separator run behind the root is measured with it, so the walk down a sloppily
    // written path ends on the root all the same ---
    {
        coid::token path = "\\\\server\\\\share"_T;
        uint32 root_length = directory::get_path_root_length_internal(path);

        DASSERTX(root_length == 10, "The run behind the server should be measured with the root");
        DASSERTX(!directory::is_separator(path.shifted_start(root_length).first_char()),
            "The part below the root should never start with a separator");

        coid::token result;
        bool ok = directory::extract_path_component_internal(path, root_length, directory::path_component_enum::last, result, path);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "share"_T, "The share is an ordinary component below the root");
        DASSERTX(path == "\\\\server"_T, "The run between the two should go with the component, leaving the bare root");

        ok = directory::extract_path_component_internal(path, root_length, directory::path_component_enum::last, result, path);
        DASSERTX(ok, "Consuming the root should report a valid component");
        DASSERTX(result == "\\\\server"_T, "The measured root should come back whole, however it was written");
        DASSERTX(path.is_empty(), "There should be nothing left once the root is consumed");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_extract_path_component_internal()
{
    using coid::directory;
    using component = coid::directory::path_component_enum;

    //the root length is an in out parameter, a caller keeps one of these across the whole walk and
    //hands it to every call, the function zeroing it once the root has been consumed
    uint32 root_length = 0;
    coid::token result;
    coid::token remainder;
    bool ok = false;

    // --- a zero root length means the path is relative and the whole of it is cut into components.
    // The rooted cases, with the root length measured by the caller, come further down ---

    // --- the separator in front of a `last` component is removed with it ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo/bar/baz.txt"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "baz.txt"_T, "The last component should be the file name");
        DASSERTX(remainder == "foo/bar"_T, "The remainder should come back without a trailing separator");
    }

    // --- the separator behind a `first` component is removed with it ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo/bar/baz.txt"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "foo"_T, "The first component should be the leading segment");
        DASSERTX(remainder == "bar/baz.txt"_T, "The remainder should come back without a leading separator");
    }

    // --- a relative path leaves the root length alone, there being no root to consume ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo/bar"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(root_length == 0, "A relative path should leave the root length at zero");
    }

    // --- an empty path is what ends a walk, the call reporting that no component was left ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal(""_T, root_length, component::last, result, remainder);
        DASSERTX(!ok, "An empty path should report that no component is left");
        DASSERTX(result.is_empty(), "There should be no component to hand back");
        DASSERTX(remainder.is_empty(), "There should be nothing left over either");
    }

    // --- a single component path comes back whole, with an empty remainder that ends the walk ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("baz.txt"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "baz.txt"_T, "A single component path should be returned whole for `last`");
        DASSERTX(remainder.is_empty(), "The remainder should be empty when there is no separator to split on");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("baz.txt"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "baz.txt"_T, "A single component path should be returned whole for `first` too");
        DASSERTX(remainder.is_empty(), "The remainder should be empty when there is no separator to split on");
    }

    // --- a single character component is a valid path, not a degenerate case ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "a"_T, "A single character path should be returned whole");
        DASSERTX(remainder.is_empty(), "The remainder for a single character path should be empty");
    }

    // --- a trailing separator is not a component of its own, and a `last` component leaves nothing
    // for it to stay on ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo/bar/"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "bar"_T, "A trailing separator should not produce an empty last component");
        DASSERTX(remainder == "foo"_T, "The remainder should come back without a trailing separator");
    }

    // --- a `first` component leaves the trailing separator of the input untouched, only the run
    // between the component and the remainder is removed ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo/bar/"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "foo"_T, "The first component should be the leading segment");
        DASSERTX(remainder == "bar/"_T, "The trailing separator of the input should stay on the remainder");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a/b/c/"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "a"_T, "The first component should be the leading segment");
        DASSERTX(remainder == "b/c/"_T, "Only the separator between the component and the remainder should be removed");
    }

    // --- with a single component the trailing separator is the one between it and the remainder,
    // so it goes either way and leaves nothing over ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a/"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "a"_T, "A trailing separator should not swallow the only component");
        DASSERTX(remainder.is_empty(), "The remainder for \"a/\" should be empty, not the separator itself");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a/"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "a"_T, "\"a/\" should yield \"a\" as the first component too");
        DASSERTX(remainder.is_empty(), "The remainder for \"a/\" (first) should be empty");
    }

    // --- a whole run of separators is removed at once, wherever it sits ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a//b"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "b"_T, "A separator run should not produce an empty component");
        DASSERTX(remainder == "a"_T, "The whole run should be removed for `last`");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a//b"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "a"_T, "A separator run should not produce an empty component");
        DASSERTX(remainder == "b"_T, "The whole run should be removed for `first`");
    }

    // --- a trailing run is not a component either ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a/b//"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "b"_T, "A trailing separator run should not produce an empty component");
        DASSERTX(remainder == "a"_T, "The remainder should come back without a trailing separator");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a//b//c"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "a"_T, "A separator run should not produce an empty component");
        DASSERTX(remainder == "b//c"_T, "Only the run the component is taken across should be removed");
    }

    // --- backslash is only a separator on Windows; elsewhere it's an ordinary filename character ---
#ifdef SYSTYPE_WIN
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo\\bar\\baz.txt"_T, root_length, component::last, result, remainder);
        DASSERTX(result == "baz.txt"_T, "The last component should be the file name (backslash separators)");
        DASSERTX(remainder == "foo\\bar"_T, "The remainder should come back without the trailing backslash separator");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo\\bar\\baz.txt"_T, root_length, component::first, result, remainder);
        DASSERTX(result == "foo"_T, "The first component should be the leading segment (backslash separators)");
        DASSERTX(remainder == "bar\\baz.txt"_T, "The remainder should not have a leading backslash separator");
    }

    // --- the two separator styles mix freely ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo/bar\\baz.txt"_T, root_length, component::last, result, remainder);
        DASSERTX(result == "baz.txt"_T, "Mixed separators should be recognized alike");
        DASSERTX(remainder == "foo/bar"_T, "The separator style inside the remainder should be left alone");
    }

    // --- a trailing backslash behaves the same way a trailing slash does ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo\\bar\\"_T, root_length, component::last, result, remainder);
        DASSERTX(result == "bar"_T, "A trailing backslash should not produce an empty last component");
        DASSERTX(remainder == "foo"_T, "The remainder should come back without a trailing backslash");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("foo\\bar\\"_T, root_length, component::first, result, remainder);
        DASSERTX(result == "foo"_T, "The first component should be the leading segment");
        DASSERTX(remainder == "bar\\"_T, "The trailing backslash of the input should stay on the remainder");
    }
#else
    {
        // no '/' present, so the whole string is a single component; backslash must not split it
        root_length = 0;
        ok = directory::extract_path_component_internal("foo\\bar\\baz.txt"_T, root_length, component::last, result, remainder);
        DASSERTX(result == "foo\\bar\\baz.txt"_T, "Backslash should not act as a separator on non-Windows systems");
        DASSERTX(remainder.is_empty(), "No '/' present, so there is nothing to split off into a remainder");
    }
#endif

    // --- dot segments are ordinary components, returned as they are and never resolved ---
    {
        root_length = 0;
        ok = directory::extract_path_component_internal("./a"_T, root_length, component::first, result, remainder);
        DASSERTX(result == "."_T, "The current dir segment should come back as an ordinary component");
        DASSERTX(remainder == "a"_T, "The remainder should be the rest of the path");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a/../b"_T, root_length, component::first, result, remainder);
        DASSERTX(result == "a"_T, "The parent segment should not consume the component in front of it");
        DASSERTX(remainder == "../b"_T, "The parent segment should be left in the remainder unresolved");
    }

    {
        root_length = 0;
        ok = directory::extract_path_component_internal("a/.."_T, root_length, component::last, result, remainder);
        DASSERTX(result == ".."_T, "The parent segment should come back as an ordinary component");
        DASSERTX(remainder == "a"_T, "The remainder should be the path in front of it, unresolved");
    }

    // --- aliasing: the path and the remainder may be the same token, the component is captured first ---
    {
        coid::token p = "foo/bar/baz.txt"_T;
        root_length = 0;
        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(result == "baz.txt"_T, "An in-place call should still return the correct last component");
        DASSERTX(p == "foo/bar"_T, "An in-place call should leave the aliased path holding the remainder");
    }

    {
        coid::token p = "foo/bar/baz.txt"_T;
        root_length = 0;
        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(result == "foo"_T, "An in-place call should still return the correct first component");
        DASSERTX(p == "bar/baz.txt"_T, "An in-place call should leave the aliased path holding the remainder");
    }

    // --- iteration: the walk ends when the call reports that no component is left ---
    {
        coid::token p = "a/b/c"_T;
        root_length = 0;

        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(ok && result == "a"_T, "The walk should start at the first component");
        DASSERTX(p == "b/c"_T, "The remainder should be the rest of the path");

        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(ok && result == "b"_T, "The walk should continue with the next component");
        DASSERTX(p == "c"_T, "The remainder should be the rest of the path");

        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(ok && result == "c"_T, "The walk should end on the last component");
        DASSERTX(p.is_empty(), "The remainder should be empty once the last component is taken");

        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(!ok, "One call past the end should report that no component is left");
    }

    {
        coid::token p = "a/b/c"_T;
        root_length = 0;

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "c"_T, "The backwards walk should start at the last component");
        DASSERTX(p == "a/b"_T, "The remainder should come back without a trailing separator");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "b"_T, "The backwards walk should continue with the next component");
        DASSERTX(p == "a"_T, "The remainder should come back without a trailing separator");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "a"_T, "The backwards walk should end on the first component");
        DASSERTX(p.is_empty(), "The remainder should be empty once the first component is taken");
    }

    // --- a path written with trailing and repeated separators walks down to the same components ---
    {
        coid::token p = "a//b/c/"_T;
        root_length = 0;

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "c"_T, "The trailing separator should not produce an empty component");
        DASSERTX(p == "a//b"_T, "The remainder should come back without a trailing separator");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "b"_T, "The walk should continue with the next component");
        DASSERTX(p == "a"_T, "The whole separator run should be removed with the component");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "a"_T, "The walk should end on the first component");
        DASSERTX(p.is_empty(), "The remainder should be empty once the first component is taken");
    }

    // --- the components concatenate back into the path they came from, one separator per join ---
    {
        coid::token p = "a/b/c"_T;
        coid::charstr rebuilt;
        root_length = 0;

        while (directory::extract_path_component_internal(p, root_length, component::first, result, p))
        {
            if (rebuilt.is_set())
                rebuilt << '/';
            rebuilt << result;
        }

        DASSERTX(coid::token(rebuilt) == "a/b/c"_T, "The components should concatenate back into the original path");
    }

    // --- the same walk over a sloppily written path rebuilds the normalized form of it ---
    {
        coid::token p = "a//b/c/"_T;
        coid::charstr rebuilt;
        root_length = 0;

        while (directory::extract_path_component_internal(p, root_length, component::first, result, p))
        {
            if (rebuilt.is_set())
                rebuilt << '/';
            rebuilt << result;
        }

        DASSERTX(coid::token(rebuilt) == "a/b/c"_T, "The separator runs and the trailing separator should be gone from the rebuilt path");
    }

    // --- note: a rooted path passed with a zero root length would have its root cut into ordinary
    // components, the root never being looked at here, only skipped. That is a caller error and
    // trips the debug guard at the top of the function, so it is not exercised by a plain call ---

    // --- the rooted cases are split by platform: the root comes back as the text it is written
    // with on windows, while a unix root is nothing but a separator run and comes back empty ---
#ifdef SYSTYPE_WIN

    // --- a `first` component of a rooted path is the root, taken without the separator run behind
    // it, and the call consumes it: the root length comes back zeroed for the calls that follow ---
    {
        root_length = 3;
        ok = directory::extract_path_component_internal("C:/a/b"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "Consuming the root should report a valid component");
        DASSERTX(result == "C:"_T, "The first component of a rooted path is the root");
        DASSERTX(remainder == "a/b"_T, "The remainder should be the whole of the path below the root");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    // --- the same variable drives the rest of the walk, the path below the root being relative ---
    {
        coid::token p = "C:/a/b"_T;
        root_length = 3;

        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(ok && result == "C:"_T, "The walk should start at the root");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");

        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(ok && result == "a"_T, "The walk should continue below the root");

        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(ok && result == "b"_T, "The walk should end on the last component");
        DASSERTX(p.is_empty(), "The remainder should be empty once the last component is taken");

        ok = directory::extract_path_component_internal(p, root_length, component::first, result, p);
        DASSERTX(!ok, "One call past the end should report that no component is left");
    }

    // --- a unc root keeps the two leading separators it is written with, they are a part of it ---
    {
        root_length = 9;
        ok = directory::extract_path_component_internal("//server/share/a"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "Consuming the root should report a valid component");
        DASSERTX(result == "//server"_T, "The first component of a unc path is the server, leading separators included");
        DASSERTX(remainder == "share/a"_T, "The share should be left at the head of the remainder");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    // --- the separator run behind the root belongs to the root length, not to the component ---
    {
        root_length = 4;
        ok = directory::extract_path_component_internal("C://a"_T, root_length, component::first, result, remainder);
        DASSERTX(result == "C:"_T, "The root should come back without the separator run behind it");
        DASSERTX(remainder == "a"_T, "The remainder should start below the run");
    }

    // --- a trailing separator of the input is left alone here as well ---
    {
        root_length = 3;
        ok = directory::extract_path_component_internal("C:/a/b/"_T, root_length, component::first, result, remainder);
        DASSERTX(result == "C:"_T, "The first component of a rooted path is the root");
        DASSERTX(remainder == "a/b/"_T, "The trailing separator of the input should stay on the remainder");
    }

    // --- a path that is nothing but a root has that root for its only component, whichever end is
    // asked for, the root being consumed either way ---
    {
        root_length = 3;
        ok = directory::extract_path_component_internal("C:/"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A path that is nothing but a root should still report a valid component");
        DASSERTX(result == "C:"_T, "A path that is nothing but a root should come back as the root");
        DASSERTX(remainder.is_empty(), "There should be nothing left once the root is reached");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    {
        root_length = 2;
        ok = directory::extract_path_component_internal("C:"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "A root without a separator behind it should still report a valid component");
        DASSERTX(result == "C:"_T, "A root without a separator behind it should come back whole");
        DASSERTX(remainder.is_empty(), "There should be nothing left once the root is reached");
    }

    // --- a `last` component is cut below the root, which stays at the head of the remainder. The
    // root length is left alone here, the root not having been reached yet ---
    {
        root_length = 3;
        ok = directory::extract_path_component_internal("C:/a/b"_T, root_length, component::last, result, remainder);
        DASSERTX(ok, "A component should have been extracted");
        DASSERTX(result == "b"_T, "The last component is an ordinary one, cut below the root");
        DASSERTX(remainder == "C:/a"_T, "The root should stay on the remainder");
        DASSERTX(root_length == 3, "The root has not been reached, its length should be left alone");
    }

    {
        root_length = 3;
        ok = directory::extract_path_component_internal("C:/a/b/"_T, root_length, component::last, result, remainder);
        DASSERTX(result == "b"_T, "A trailing separator should not produce an empty last component");
        DASSERTX(remainder == "C:/a"_T, "The root should stay on the remainder");
    }

    // --- the separator run between the root and the only component below it goes with the
    // component, leaving the bare root, which the next call consumes ---
    {
        root_length = 3;
        ok = directory::extract_path_component_internal("C:/a"_T, root_length, component::last, result, remainder);
        DASSERTX(result == "a"_T, "The only component below the root should be cut off");
        DASSERTX(remainder == "C:"_T, "The remainder should be the bare root the next call consumes");
    }

    {
        root_length = 4;
        ok = directory::extract_path_component_internal("C://a"_T, root_length, component::last, result, remainder);
        DASSERTX(result == "a"_T, "The component below the root should be cut off");
        DASSERTX(remainder == "C:"_T, "The whole run between the two should go with the component");
    }

    // --- a whole backwards walk down a drive path, the root length carried through it ---
    {
        coid::token p = "C:/a/b"_T;
        root_length = 3;

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "b"_T, "The walk should start at the last component");
        DASSERTX(p == "C:/a"_T, "The root should stay at the head of the remainder");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "a"_T, "The walk should continue with the next component");
        DASSERTX(p == "C:"_T, "The remainder should be the bare root");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "C:"_T, "The walk should end at the root");
        DASSERTX(p.is_empty(), "There should be nothing left once the root is consumed");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    // --- the same walk down a unc path ---
    {
        coid::token p = "//server/share/a"_T;
        root_length = 9;

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "a"_T, "The walk should start at the last component");
        DASSERTX(p == "//server/share"_T, "The root should stay at the head of the remainder");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "share"_T, "The share is an ordinary component below the root");
        DASSERTX(p == "//server"_T, "The remainder should be the bare root");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "//server"_T, "The walk should end at the root");
        DASSERTX(p.is_empty(), "There should be nothing left once the root is consumed");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    // --- the separator style of the root is no more looked at than the rest of it ---
    {
        root_length = 3;
        ok = directory::extract_path_component_internal("C:\\a\\b"_T, root_length, component::first, result, remainder);
        DASSERTX(result == "C:"_T, "The first component of a rooted path is the root");
        DASSERTX(remainder == "a\\b"_T, "The remainder should be the whole of the path below the root");
    }

    {
        root_length = 9;
        ok = directory::extract_path_component_internal("\\\\server\\share\\a"_T, root_length, component::first, result, remainder);
        DASSERTX(result == "\\\\server"_T, "The first component of a unc path is the server, leading separators included");
        DASSERTX(remainder == "share\\a"_T, "The share should be left at the head of the remainder");
    }

    {
        root_length = 9;
        ok = directory::extract_path_component_internal("\\\\server\\share\\a"_T, root_length, component::last, result, remainder);
        DASSERTX(result == "a"_T, "The last component is an ordinary one, cut below the root");
        DASSERTX(remainder == "\\\\server\\share"_T, "The root should stay on the remainder");
    }
#else
    // --- the unix root is nothing but a separator run, so it comes back as an empty component,
    // which is what tells a root apart from an ordinary one ---
    {
        root_length = 1;
        ok = directory::extract_path_component_internal("/a/b"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "Consuming the root should report a valid component");
        DASSERTX(result.is_empty(), "The unix root is an empty component");
        DASSERTX(remainder == "a/b"_T, "The remainder should be the whole of the path below the root");
        DASSERTX(root_length == 0, "The consumed root should leave the root length at zero");
    }

    {
        root_length = 3;
        ok = directory::extract_path_component_internal("///a/b"_T, root_length, component::first, result, remainder);
        DASSERTX(ok, "Consuming the root should report a valid component");
        DASSERTX(result.is_empty(), "A whole leading run is the root, and an empty component all the same");
        DASSERTX(remainder == "a/b"_T, "The remainder should be the whole of the path below the root");
    }

    // --- a `last` component takes the separator in front of it along, the unix root with it, so the
    // walk ends one call earlier and the empty root component is never reported for this end ---
    {
        coid::token p = "/a/b"_T;
        root_length = 1;

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "b"_T, "The walk should start at the last component");
        DASSERTX(p == "/a"_T, "The leading separator should stay on the remainder");

        ok = directory::extract_path_component_internal(p, root_length, component::last, result, p);
        DASSERTX(ok && result == "a"_T, "The walk should continue with the next component");
        DASSERTX(p.is_empty(), "The separator run in front of the component goes with it, root and all");
    }
#endif
}

void directory_tests::test_append_path()
{
    using coid::directory;

    const char sep = directory::separator();

    // --- a base without a trailing separator gets the platform separator inserted ---
    {
        coid::charstr dst = "base";
        const bool ok = directory::append_path(dst, "leaf"_T);

        coid::charstr expected = "base";
        expected << sep << "leaf";

        DASSERTX(ok, "Appending a plain relative component should succeed");
        DASSERTX(coid::token(dst) == coid::token(expected), "A separator should be inserted between the base and the component");
    }

    // --- a separator already present on the base is reused instead of the platform one ---
    {
        coid::charstr dst = "base/";
        const bool ok = directory::append_path(dst, "leaf"_T);

        DASSERTX(ok, "Appending to a base ending with a separator should succeed");
        DASSERTX(coid::token(dst) == "base/leaf"_T, "The separator style of the base should be preserved");
    }

    // --- multiple segments are appended verbatim ---
    {
        coid::charstr dst = "base/";
        const bool ok = directory::append_path(dst, "sub/leaf"_T);

        DASSERTX(ok, "Appending a multi segment path should succeed");
        DASSERTX(coid::token(dst) == "base/sub/leaf"_T, "All segments should be appended");
    }

    // --- an empty base takes the path as is, with no leading separator ---
    {
        coid::charstr dst;
        const bool ok = directory::append_path(dst, "leaf"_T);

        DASSERTX(ok, "Appending to an empty base should succeed");
        DASSERTX(coid::token(dst) == "leaf"_T, "An empty base must not produce a leading separator");
    }

    // --- a parent segment is appended as it is, nothing is resolved without keep_below ---
    {
        coid::charstr dst = "a/b/c/";
        const bool ok = directory::append_path(dst, "../d"_T);

        DASSERTX(ok, "A parent segment should succeed");
        DASSERTX(coid::token(dst) == "a/b/c/../d"_T, "The parent segment should be kept, with no separator added");
    }

    {
        coid::charstr dst = "a/b/c";
        const bool ok = directory::append_path(dst, ".."_T);

        coid::charstr expected = "a/b/c";
        expected << sep << "..";

        DASSERTX(ok, "A lone parent segment should succeed");
        DASSERTX(coid::token(dst) == coid::token(expected), "A lone parent segment should be appended like any other component");
    }

    // --- without keep_below there is nothing to fail against, a path leading above the base is
    // appended the same way ---
    {
        coid::charstr dst = "a";
        const bool ok = directory::append_path(dst, "../../b"_T);

        coid::charstr expected = "a";
        expected << sep << "../../b";

        DASSERTX(ok, "Escaping above the base is not an error without keep_below");
        DASSERTX(coid::token(dst) == coid::token(expected), "The path should be appended as it was given");
    }

    // --- only a whole component of two dots is a parent segment, a name may start with them ---
    {
        coid::charstr dst = "a/";
        const bool ok = directory::append_path(dst, "..b"_T);

        DASSERTX(ok, "A component starting with two dots is an ordinary name and should be appended");
        DASSERTX(coid::token(dst) == "a/..b"_T, "The name should be appended as it is");
    }

    {
        coid::charstr dst = "a/b/";
        const bool ok = directory::append_path(dst, "../..abc/d"_T);

        DASSERTX(ok, "A parent segment followed by such a name should succeed");
        DASSERTX(coid::token(dst) == "a/b/../..abc/d"_T, "The path should be appended as it was given");
    }

    // --- an absolute path replaces the base entirely ---
    {
#ifdef SYSTYPE_WIN
        coid::charstr dst = "C:/base/";
        const bool ok = directory::append_path(dst, "D:/other"_T);

        DASSERTX(ok, "Appending an absolute path should succeed");
        DASSERTX(coid::token(dst) == "D:/other"_T, "An absolute path should replace the base");
#else
        coid::charstr dst = "/base/";
        const bool ok = directory::append_path(dst, "/other"_T);

        DASSERTX(ok, "Appending an absolute path should succeed");
        DASSERTX(coid::token(dst) == "/other"_T, "An absolute path should replace the base");
#endif
    }

    // --- keep_below accepts a path whose parent segments stay within the appended part, and the
    // result comes back compacted. The OS default separator that joined the component the parent
    // segment consumes goes with it, the component left carries the one written in front of it ---
    {
        coid::charstr dst = "a/b";
        const bool ok = directory::append_path(dst, "c/../d"_T, true);

        DASSERTX(ok, "A path that never escapes its own scope should be accepted");
        DASSERTX(coid::token(dst) == "a/b/d"_T, "The component should be joined with the separator written in front of it");
    }

    // --- keep_below rejects an escaping path and leaves the destination alone ---
    {
        coid::charstr dst = "a/b";
        const bool ok = directory::append_path(dst, "c/../../../d"_T, true);

        DASSERTX(!ok, "A path escaping the base scope should be rejected");
        DASSERTX(coid::token(dst) == "a/b"_T, "A rejected path must leave the destination unchanged");
    }

    // --- a leading parent segment that lands somewhere else is rejected ---
    {
        coid::charstr dst = "a/b";
        const bool ok = directory::append_path(dst, "../d"_T, true);

        DASSERTX(!ok, "A parent segment leading outside the base scope should be rejected");
        DASSERTX(coid::token(dst) == "a/b"_T, "A rejected path must leave the destination unchanged");
    }

    // --- a leading parent segment leading back through the same component is accepted ---
    {
        coid::charstr dst = "a/b";
        const bool ok = directory::append_path(dst, "../b/c"_T, true);

        DASSERTX(ok, "A parent segment that leads back below the base scope should be accepted");
        DASSERTX(coid::token(dst) == "a/b/c"_T, "The resolved path should be the one below the base");
    }

    // --- leading back through a different component of the same depth is still an escape ---
    {
        coid::charstr dst = "a/b/c";
        const bool ok = directory::append_path(dst, "../../x/c"_T, true);

        DASSERTX(!ok, "Leading back through different components should be rejected");
        DASSERTX(coid::token(dst) == "a/b/c"_T, "A rejected path must leave the destination unchanged");
    }

    // --- leading back through the same components at depth is accepted ---
    {
        coid::charstr dst = "a/b/c";
        const bool ok = directory::append_path(dst, "../../b/c/d"_T, true);

        DASSERTX(ok, "Leading back through the same components should be accepted");
        DASSERTX(coid::token(dst) == "a/b/c/d"_T, "The resolved path should be the one below the base");
    }

    // --- a lone parent segment can never stay below the base ---
    {
        coid::charstr dst = "a/b/c";
        const bool ok = directory::append_path(dst, ".."_T, true);

        DASSERTX(!ok, "A lone parent segment resolves above the base and should be rejected");
        DASSERTX(coid::token(dst) == "a/b/c"_T, "A rejected path must leave the destination unchanged");
    }

    // --- without keep_below the same path is accepted, and appended as it is ---
    {
        coid::charstr dst = "a/b";
        const bool ok = directory::append_path(dst, "../d"_T);

        coid::charstr expected = "a/b";
        expected << sep << "../d";

        DASSERTX(ok, "Without keep_below an escaping path is accepted");
        DASSERTX(coid::token(dst) == coid::token(expected), "Without keep_below the escaping path is appended as it was given");
    }

    // --- keep_below rejects an absolute path that does not lie below the base ---
    {
#ifdef SYSTYPE_WIN
        coid::charstr dst = "C:/base";
        const bool ok = directory::append_path(dst, "D:/other"_T, true);

        DASSERTX(!ok, "An absolute path outside the base scope should be rejected");
        DASSERTX(coid::token(dst) == "C:/base"_T, "A rejected absolute path must leave the destination unchanged");
#else
        coid::charstr dst = "/base";
        const bool ok = directory::append_path(dst, "/other"_T, true);

        DASSERTX(!ok, "An absolute path outside the base scope should be rejected");
        DASSERTX(coid::token(dst) == "/base"_T, "A rejected absolute path must leave the destination unchanged");
#endif
    }

    // --- keep_below accepts an absolute path that does lie below the base ---
    {
#ifdef SYSTYPE_WIN
        coid::charstr dst = "C:/base";
        const bool ok = directory::append_path(dst, "C:/base/leaf"_T, true);

        DASSERTX(ok, "An absolute path below the base scope should be accepted");
        DASSERTX(coid::token(dst) == "C:/base/leaf"_T, "An accepted absolute path should replace the base");
#else
        coid::charstr dst = "/base";
        const bool ok = directory::append_path(dst, "/base/leaf"_T, true);

        DASSERTX(ok, "An absolute path below the base scope should be accepted");
        DASSERTX(coid::token(dst) == "/base/leaf"_T, "An accepted absolute path should replace the base");
#endif
    }
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_make_path()
{
    using coid::directory;

    const char sep = directory::separator();

    // --- the common case matches append_path on a copy of the base ---
    {
        const coid::charstr path = directory::make_path("base/"_T, "leaf"_T);
        DASSERTX(coid::token(path) == "base/leaf"_T, "make_path should append the component to the base");
    }

    // --- the base is never modified, it is taken by token ---
    {
        const coid::charstr base = "base/";
        const coid::charstr path = directory::make_path(base, "leaf"_T);

        DASSERTX(coid::token(base) == "base/"_T, "make_path must not modify the base");
        DASSERTX(coid::token(path) == "base/leaf"_T, "make_path should return the resolved path");
    }

    // --- without keep_below a path leading above the base is produced, not reported as a failure ---
    {
        const coid::charstr path = directory::make_path("a"_T, "../../b"_T);

        coid::charstr expected = "a";
        expected << sep << "../../b";

        DASSERTX(coid::token(path) == coid::token(expected), "The path should be appended as it was given");
    }

    // --- a name starting with two dots is ordinary, it is not a failure ---
    {
        const coid::charstr path = directory::make_path("a/"_T, "..b"_T);
        DASSERTX(coid::token(path) == "a/..b"_T, "A component starting with two dots should be appended as it is");
    }

    // --- without keep_below nothing is resolved, as append_path does not resolve either ---
    {
        const coid::charstr path = directory::make_path("a/b"_T, "../d"_T);

        coid::charstr expected = "a/b";
        expected << sep << "../d";

        DASSERTX(coid::token(path) == coid::token(expected), "Without keep_below the parent segment should be left where it is");
    }

    // --- with keep_below an escaping path yields an empty result ---
    {
        const coid::charstr path = directory::make_path("a/b"_T, "../d"_T, true);
        DASSERTX(path.is_empty(), "keep_below should reject a path escaping the base scope");
    }

    // --- a path that stays within its own scope is accepted under keep_below, and comes back
    // compacted, the OS default separator going with the component the parent segment consumes ---
    {
        const coid::charstr path = directory::make_path("a/b"_T, "c/../d"_T, true);

        DASSERTX(coid::token(path) == "a/b/d"_T, "The component should be joined with the separator written in front of it");
    }

    // --- an absolute path silently replaces the base, keep_below must reject it ---
    {
#ifdef SYSTYPE_WIN
        const coid::charstr escaping = directory::make_path("C:/saves"_T, "D:/other"_T, true);
        DASSERTX(escaping.is_empty(), "keep_below should reject an absolute path outside the base");

        const coid::charstr below = directory::make_path("C:/saves"_T, "C:/saves/leaf"_T, true);
        DASSERTX(coid::token(below) == "C:/saves/leaf"_T, "An absolute path below the base should be accepted");

        const coid::charstr replaced = directory::make_path("C:/saves"_T, "D:/other"_T);
        DASSERTX(coid::token(replaced) == "D:/other"_T, "Without keep_below an absolute path replaces the base");
#else
        const coid::charstr escaping = directory::make_path("/saves"_T, "/other"_T, true);
        DASSERTX(escaping.is_empty(), "keep_below should reject an absolute path outside the base");

        const coid::charstr below = directory::make_path("/saves"_T, "/saves/leaf"_T, true);
        DASSERTX(coid::token(below) == "/saves/leaf"_T, "An absolute path below the base should be accepted");

        const coid::charstr replaced = directory::make_path("/saves"_T, "/other"_T);
        DASSERTX(coid::token(replaced) == "/other"_T, "Without keep_below an absolute path replaces the base");
#endif
    }
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_compact_path()
{
    using coid::directory;

    // --- a path with nothing to compact comes back unchanged ---
    {
        coid::charstr path = "a/b/c";
        DASSERTX(directory::compact_path(path), "A plain relative path should compact");
        DASSERTX(coid::token(path) == "a/b/c"_T, "A path with nothing to compact should stay as it is");
    }

    // --- repeated separators collapse into one ---
    {
        coid::charstr path = "a//b";
        DASSERTX(directory::compact_path(path), "A path with a doubled separator should compact");
        DASSERTX(coid::token(path) == "a/b"_T, "A doubled separator should collapse into one");
    }

    {
        coid::charstr path = "a///b////c";
        DASSERTX(directory::compact_path(path), "A path with separator runs should compact");
        DASSERTX(coid::token(path) == "a/b/c"_T, "Every separator run should collapse into one");
    }

    // --- current dir segments are removed ---
    {
        coid::charstr path = "a/./b";
        DASSERTX(directory::compact_path(path), "A path with a current dir segment should compact");
        DASSERTX(coid::token(path) == "a/b"_T, "A current dir segment should be removed");
    }

    {
        coid::charstr path = "./a";
        DASSERTX(directory::compact_path(path), "A path starting with a current dir segment should compact");
        DASSERTX(coid::token(path) == "a"_T, "A leading current dir segment should be removed");
    }

    // --- parent segments consume the component before them ---
    {
        coid::charstr path = "a/b/../c";
        DASSERTX(directory::compact_path(path), "A path with a parent segment should compact");
        DASSERTX(coid::token(path) == "a/c"_T, "A parent segment should consume the preceding component");
    }

    {
        coid::charstr path = "a/../b";
        DASSERTX(directory::compact_path(path), "A parent segment right after the first component should compact");
        DASSERTX(coid::token(path) == "b"_T, "The whole leading component should be consumed");
    }

    // --- a trailing parent segment leaves the shortened path ---
    {
        coid::charstr path = "a/b/..";
        DASSERTX(directory::compact_path(path), "A trailing parent segment should compact");
        DASSERTX(coid::token(path) == "a"_T, "A trailing parent segment should consume the preceding component");
    }

    // --- a relative path may legitimately point above itself, the leading parents are kept ---
    {
        coid::charstr path = "../a";
        DASSERTX(directory::compact_path(path), "A relative path leading up should compact");
        DASSERTX(coid::token(path) == "../a"_T, "A leading parent segment of a relative path has nothing to consume");
    }

    {
        coid::charstr path = "a/../../b";
        DASSERTX(directory::compact_path(path), "A relative path leading above its own root should compact");
        DASSERTX(coid::token(path) == "../b"_T, "Only the parent segments with something to consume should be resolved");
    }

    // --- only a whole component of two dots is a parent segment, a name may start with them ---
    {
        coid::charstr path = "a/..b/c";
        DASSERTX(directory::compact_path(path), "A path with a component starting with two dots should compact");
        DASSERTX(coid::token(path) == "a/..b/c"_T, "A component starting with two dots is an ordinary name");
    }

    {
        coid::charstr path = "../..abc/d";
        DASSERTX(directory::compact_path(path), "A parent segment followed by such a name should compact");
        DASSERTX(coid::token(path) == "../..abc/d"_T, "Only the leading component is a parent segment here");
    }

    // --- such a name is a forward component, so a parent segment consumes it like any other ---
    {
        coid::charstr path = "a/..b/../c";
        DASSERTX(directory::compact_path(path), "A parent segment after such a name should compact");
        DASSERTX(coid::token(path) == "a/c"_T, "A name starting with two dots should be consumed like any other component");
    }

    // --- a trailing separator is preserved ---
    {
        coid::charstr path = "a/b/";
        DASSERTX(directory::compact_path(path), "A path with a trailing separator should compact");
        DASSERTX(coid::token(path) == "a/b/"_T, "A trailing separator should be preserved");
    }

    // --- an empty path is not an error ---
    {
        coid::charstr path;
        DASSERTX(directory::compact_path(path), "An empty path should not be reported as an error");
        DASSERTX(path.is_empty(), "An empty path should stay empty");
    }

    // --- tosep rewrites the separators that are kept ---
    {
        coid::charstr path = "a/b/../c";
        DASSERTX(directory::compact_path(path, '\\'), "Compacting with a target separator should succeed");
        DASSERTX(coid::token(path) == "a\\c"_T, "The separators of the result should be rewritten");
    }

#ifdef SYSTYPE_WIN

    // --- on windows both separator styles are recognized and can be normalized ---
    {
        coid::charstr path = "a/b\\c";
        DASSERTX(directory::compact_path(path, '/'), "A path with mixed separators should compact");
        DASSERTX(coid::token(path) == "a/b/c"_T, "Mixed separators should be normalized to the target one");
    }

    // --- a drive path is absolute and keeps its root ---
    {
        coid::charstr path = "C:\\a\\b";
        DASSERTX(directory::compact_path(path), "A drive path should compact");
        DASSERTX(coid::token(path) == "C:\\a\\b"_T, "A drive path with nothing to compact should stay as it is");
    }

    {
        coid::charstr path = "C:\\a\\..\\b";
        DASSERTX(directory::compact_path(path), "A drive path with a parent segment should compact");
        DASSERTX(coid::token(path) == "C:\\b"_T, "A parent segment should consume the preceding component");
    }

    // --- a bare drive has no path part to compact ---
    {
        coid::charstr path = "C:";
        DASSERTX(directory::compact_path(path), "A bare drive should compact");
        DASSERTX(coid::token(path) == "C:"_T, "A bare drive should stay as it is");
    }

    {
        coid::charstr path = "C:\\";
        DASSERTX(directory::compact_path(path), "A drive root should compact");
        DASSERTX(coid::token(path) == "C:\\"_T, "A drive root should stay as it is");
    }

    // --- a drive letter not followed by a separator is malformed ---
    {
        coid::charstr path = "C:a";
        DASSERTX(!directory::compact_path(path), "A drive letter not followed by a separator should be rejected");
    }

    // --- a unc path keeps its share as the root ---
    {
        coid::charstr path = "\\\\server\\share\\a\\..\\b";
        DASSERTX(directory::compact_path(path), "A unc path should compact");
        DASSERTX(coid::token(path) == "\\\\server\\share\\b"_T, "A parent segment should consume the preceding component");
    }

    // --- stepping above the root of a unc path is an error ---
    {
        coid::charstr path = "\\\\server\\..\\..\\x";
        DASSERTX(!directory::compact_path(path), "Stepping above the root of a unc path should be rejected");
    }

    // --- stepping above the root of a drive is an error, same as it is for a unc path ---
    {
        coid::charstr path = "C:\\..\\x";
        DASSERTX(!directory::compact_path(path), "Stepping above the root of a drive should be rejected");
    }

#else

    // --- an absolute path keeps its root ---
    {
        coid::charstr path = "/a/b";
        DASSERTX(directory::compact_path(path), "An absolute path should compact");
        DASSERTX(coid::token(path) == "/a/b"_T, "An absolute path with nothing to compact should stay as it is");
    }

    {
        coid::charstr path = "/a/../b";
        DASSERTX(directory::compact_path(path), "An absolute path with a parent segment should compact");
        DASSERTX(coid::token(path) == "/b"_T, "A parent segment should consume the preceding component");
    }

    // --- stepping above the root is an error ---
    {
        coid::charstr path = "/../a";
        DASSERTX(!directory::compact_path(path), "Stepping above the root should be rejected");
    }

    {
        coid::charstr path = "/a/../../b";
        DASSERTX(!directory::compact_path(path), "Stepping above the root through a component should be rejected");
    }

    // --- a leading separator run collapses like any other ---
    {
        coid::charstr path = "//a";
        DASSERTX(directory::compact_path(path), "A path with a leading separator run should compact");
        DASSERTX(coid::token(path) == "/a"_T, "A leading separator run should collapse into one");
    }

    // --- on non windows a backslash is an ordinary filename character ---
    {
        coid::charstr path = "a/b\\c";
        DASSERTX(directory::compact_path(path, '/'), "A path containing a backslash should compact");
        DASSERTX(coid::token(path) == "a/b\\c"_T, "A backslash must not be treated as a separator");
    }

#endif //SYSTYPE_WIN
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_create_compact_path()
{
    using coid::directory;

    // --- the source path is not modified ---
    {
        const coid::charstr source = "a/b/../c";
        const coid::charstr result = directory::create_compact_path(source);

        DASSERTX(coid::token(source) == "a/b/../c"_T, "create_compact_path must not modify its input");
        DASSERTX(coid::token(result) == "a/c"_T, "create_compact_path should return the compacted path");
    }

    // --- the target separator is applied the same way as in compact_path ---
    {
        const coid::charstr result = directory::create_compact_path("a/b/../c"_T, '\\');
        DASSERTX(coid::token(result) == "a\\c"_T, "The separators of the result should be rewritten");
    }

    // --- a rejected path yields an empty result ---
    {
#ifdef SYSTYPE_WIN
        const coid::charstr result = directory::create_compact_path("C:a"_T);
        DASSERTX(result.is_empty(), "A malformed drive path should produce an empty result");
#else
        const coid::charstr result = directory::create_compact_path("/../a"_T);
        DASSERTX(result.is_empty(), "A path stepping above the root should produce an empty result");
#endif
    }

    // --- an empty result is ambiguous, it is returned for a valid empty path as well ---
    {
        const coid::charstr result = directory::create_compact_path(""_T);
        DASSERTX(result.is_empty(), "An empty path compacts to an empty result, indistinguishable from a failure");
    }
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_verify_path_syntax()
{
#ifdef SYSTYPE_WIN
    using coid::directory;
    using result = coid::directory::verify_path_syntax_result_enum;

    // --- an empty path is not a path ---
    {
        DASSERTX(directory::verify_path_syntax(""_T) == result::invalid, "An empty path should be invalid");
    }

    // --- a path without a trailing separator denotes a file ---
    {
        DASSERTX(directory::verify_path_syntax("a\\b"_T) == result::valid_relative_file_path, "A relative path should be recognized");
        DASSERTX(directory::verify_path_syntax("a"_T) == result::valid_relative_file_path, "A bare name is a relative file path");
        DASSERTX(directory::verify_path_syntax("C:\\a\\b"_T) == result::valid_absolute_file_path, "An absolute path should be recognized");
    }

    // --- a trailing separator denotes a directory ---
    {
        DASSERTX(directory::verify_path_syntax("a\\b\\"_T) == result::valid_relative_directory_path, "A trailing separator marks a directory");
        DASSERTX(directory::verify_path_syntax("C:\\a\\"_T) == result::valid_absolue_directory_path, "A trailing separator marks a directory");
    }

    // --- a bare drive is an absolute directory ---
    {
        DASSERTX(directory::verify_path_syntax("C:"_T) == result::valid_absolue_directory_path, "A bare drive is an absolute directory path");
        DASSERTX(directory::verify_path_syntax("C:\\"_T) == result::valid_absolue_directory_path, "A drive root is an absolute directory path");
    }

    // --- both separator styles are accepted ---
    {
        DASSERTX(directory::verify_path_syntax("C:/a/b"_T) == result::valid_absolute_file_path, "Forward slashes should be accepted");
    }

    // --- the current and parent dir segments are legal components ---
    {
        DASSERTX(directory::verify_path_syntax("a\\..\\b"_T) == result::valid_relative_file_path, "A parent segment is a legal component");
        DASSERTX(directory::verify_path_syntax(".\\a"_T) == result::valid_relative_file_path, "A current dir segment is a legal component");
    }

    // --- characters that cannot appear in a name make the path invalid ---
    {
        DASSERTX(directory::verify_path_syntax("a\\b?c"_T) == result::invalid, "A forbidden character should be rejected");
        DASSERTX(directory::verify_path_syntax("a\\b|c"_T) == result::invalid, "A forbidden character should be rejected");
        DASSERTX(directory::verify_path_syntax("a\\b."_T) == result::invalid, "A name ending with a dot should be rejected");
    }

    // --- a drive letter outside the accepted range is invalid ---
    {
        DASSERTX(directory::verify_path_syntax("1:\\a"_T) == result::invalid, "A drive letter must be a letter");
    }

    // --- a unc path is absolute, the same way a drive path is ---
    {
        DASSERTX(directory::verify_path_syntax("\\\\server\\share\\a"_T) == result::valid_absolute_file_path, "A unc path should be recognized as absolute");
        DASSERTX(directory::verify_path_syntax("//server/share/a"_T) == result::valid_absolute_file_path, "A unc path with forward slashes should be recognized too");
    }

    // --- the server and the share alone denote a directory ---
    {
        DASSERTX(directory::verify_path_syntax("\\\\server\\share"_T) == result::valid_absolue_directory_path, "A share is a directory");
        DASSERTX(directory::verify_path_syntax("\\\\server\\share\\"_T) == result::valid_absolue_directory_path, "A share with a trailing separator is a directory");
        DASSERTX(directory::verify_path_syntax("\\\\server"_T) == result::valid_absolue_directory_path, "A server alone is a directory");
    }

    // --- a unc path needs a server ---
    {
        DASSERTX(directory::verify_path_syntax("\\\\"_T) == result::invalid, "A bare unc prefix is not a path");
        DASSERTX(directory::verify_path_syntax("\\\\\\share"_T) == result::invalid, "A unc path with an empty server should be rejected");
    }

    // --- the components below the share are validated like any other ---
    {
        DASSERTX(directory::verify_path_syntax("\\\\server\\share\\a?b"_T) == result::invalid, "A forbidden character below the share should be rejected");
        DASSERTX(directory::verify_path_syntax("\\\\server\\sh?are\\a"_T) == result::invalid, "A forbidden character in the share should be rejected");
    }
#endif //SYSTYPE_WIN
}

////////////////////////////////////////////////////////////////////////////////

void directory_tests::test_build_path_internal()
{
    using coid::directory;
    const char sep = directory::separator();

#ifdef SYSTYPE_WIN
    // --- compacting only drive without trailing sparators---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "C:"_T, "The result should be same");
    }

    // --- compacting only drive with trailing sparators---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:\\/"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "C:/"_T, "Drive with the last separator used in the separator sequence");
    }

    // --- compacting only drive with trailing sparators with use_separor == '\\'---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:\\/"_T, ""_T, result, true, false, '\\');

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "C:\\"_T, "Drive with the separator from the param");
    }

    // --- a drive letter not followed by a separator is not a path this takes, the same way
    // verify_path_syntax and compact_path do not take it ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:a"_T, ""_T, result, true, false);

        DASSERTX(!ok, "A drive letter not followed by a separator should be rejected");
    }
#else
    // --- there is no drive nor unc path here, the root of an absolute path is the leading
    // separator run, and it is what the counterparts below are written with ---

    // --- compacting only root ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("/"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "/"_T, "The result should be same");
    }

    // --- compacting only root written as a separator run ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("///"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "/"_T, "Root with the last separator used in the separator sequence");
    }

    // --- compacting only root with use_separator == '/' ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("///"_T, ""_T, result, true, false, '/');

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "/"_T, "Root with the separator from the param");
    }

    // --- an absolute base keeps its root when compacted on its own ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("/a/../b"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting an absolute base should succeed");
        DASSERTX(coid::token(result) == "/b"_T, "The root should be kept");
    }

    // --- a base compacting above its own root is an error ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("/../a"_T, ""_T, result, true, false);

        DASSERTX(!ok, "A base climbing above its root should fail");
    }

    // --- there is no drive here, "C:a" is an ordinary one component name ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:a"_T, ""_T, result, true, false);

        DASSERTX(ok, "A name holding a colon should compact like any other");
        DASSERTX(coid::token(result) == "C:a"_T, "The name should come back as it is");
    }
#endif // SYSTYPE_WIN

    // --- with an empty appended path the call is a plain compaction of the base ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b/../c"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "a/c"_T, "The parent segment should consume the preceding component");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/./b//c"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "a/b/c"_T, "The current dir segment and the separator run should be removed");
    }

    // --- a leading current dir segment goes with the separator behind it, the component that comes
    // first has nothing in front of it to write ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("./a/b"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "a/b"_T, "A leading current dir segment should leave no separator behind");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("./a"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base on its own should succeed");
        DASSERTX(coid::token(result) == "a"_T, "A base of a single component behind a current dir segment should come back bare");
    }

    // --- with to_separator zero the separators of the base are kept as they are ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a\\b\\..\\c"_T, ""_T, result, true, false, 0);

        DASSERTX(ok, "Compacting a base written with backslashes should succeed");
        DASSERTX(coid::token(result) == "a\\c"_T, "The separator style of the base should be preserved");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b\\c"_T, ""_T, result, true, false, 0);

        DASSERTX(ok, "Compacting a base with mixed separators should succeed");
        DASSERTX(coid::token(result) == "a/b\\c"_T, "With to_separator zero the mixed separators should be left alone");
    }

    // --- with to_separator set the separators of the base are rewritten ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/./b//c"_T, ""_T, result, true, false, '\\');

        DASSERTX(ok, "Compacting a base with a target separator should succeed");
        DASSERTX(coid::token(result) == "a\\b\\c"_T, "The separators of the result should be rewritten");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a\\b\\..\\c"_T, ""_T, result, true, false, '/');

        DASSERTX(ok, "Normalizing a base to forward slashes should succeed");
        DASSERTX(coid::token(result) == "a/c"_T, "The separators of the result should be rewritten");
    }

    // --- an absolute base keeps its root when compacted on its own ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/a/../b"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting an absolute base should succeed");
        DASSERTX(coid::token(result) == "C:/b"_T, "The drive should be kept");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("//server/share/a/../b"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a unc base should succeed");
        DASSERTX(coid::token(result) == "//server/share/b"_T, "The unc root should be kept");
    }

    // --- a base compacting above its own root is an error ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/../a"_T, ""_T, result, true, false);

        DASSERTX(!ok, "A base climbing above its root should fail");
    }

    // --- a relative base may compact into a path leading up ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/../../b"_T, ""_T, result, true, false);

        DASSERTX(ok, "A relative base leading above itself should succeed");
        DASSERTX(coid::token(result) == "../b"_T, "Only the parent segment with something to consume should be resolved");
    }

    // --- the parent segments a relative base leads with have nothing to consume, they are kept
    // as they are and are not components the ones behind them could be resolved against ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("../../a/b"_T, ""_T, result, true, false);

        DASSERTX(ok, "A relative base leading up should succeed");
        DASSERTX(coid::token(result) == "../../a/b"_T, "A base of leading parent segments should come back as it is");
    }

    // --- a trailing separator marks a directory, compacting keeps it ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b/"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base ending with a separator should succeed");
        DASSERTX(coid::token(result) == "a/b/"_T, "A trailing separator should be preserved");
    }

    // --- a trailing separator run collapses into a single one ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b//"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a base ending with a separator run should succeed");
        DASSERTX(coid::token(result) == "a/b/"_T, "A trailing separator run should collapse into one");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a///b////"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting separator runs inside and at the end should succeed");
        DASSERTX(coid::token(result) == "a/b/"_T, "Every separator run should collapse into one");
    }

    // --- a parent segment resolved against a base ending with a separator keeps it ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b/../"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a parent segment with a trailing separator should succeed");
        DASSERTX(coid::token(result) == "a/"_T, "The parent segment should be resolved and the trailing separator kept");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b/c/..//"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting a parent segment with a trailing separator run should succeed");
        DASSERTX(coid::token(result) == "a/b/"_T, "The parent segment should be resolved and the run collapsed");
    }

    // --- a trailing separator survives the separator rewriting ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b//"_T, ""_T, result, true, false, '\\');

        DASSERTX(ok, "Compacting a base ending with a separator run should succeed");
        DASSERTX(coid::token(result) == "a\\b\\"_T, "The trailing separator should be rewritten along with the others");
    }

    // --- an absolute base that compacts down to its root keeps the root separator ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/a/../"_T, ""_T, result, true, false);

        DASSERTX(ok, "Compacting an absolute base down to its root should succeed");
        DASSERTX(coid::token(result) == "C:/"_T, "The root separator should be kept");
    }

    // --- without make_compact an empty appended path leaves the base as it is ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b/../c"_T, ""_T, result, false, false);

        DASSERTX(ok, "Passing a base through should succeed");
        DASSERTX(coid::token(result) == "a/b/../c"_T, "Without make_compact the base should come back as it is");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b//"_T, ""_T, result, false, false);

        DASSERTX(ok, "Passing a base ending with a separator run through should succeed");
        DASSERTX(coid::token(result) == "a/b//"_T, "Without make_compact the separator run should be left alone");
    }

    // --- to_separator applies even when nothing is compacted ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, ""_T, result, false, false, '\\');

        DASSERTX(ok, "Rewriting the separators without compacting should succeed");
        DASSERTX(coid::token(result) == "a\\b"_T, "The separators should be rewritten regardless of make_compact");
    }

    // --- an absolute appended path replaces the base ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "C:/x"_T, result, false, false);

        DASSERTX(ok, "An absolute appended path should succeed");
        DASSERTX(coid::token(result) == "C:/x"_T, "An absolute appended path should replace the base");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/a"_T, "//server/share/x"_T, result, false, false);

        DASSERTX(ok, "An absolute unc appended path should succeed");
        DASSERTX(coid::token(result) == "//server/share/x"_T, "An absolute unc path should replace the base too");
    }

    // --- keep_below applies to an absolute appended path as well ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/base"_T, "C:/other"_T, result, false, true);

        DASSERTX(!ok, "An absolute path outside the base should be rejected");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/base"_T, "C:/base/leaf"_T, result, false, true);

        DASSERTX(ok, "An absolute path below the base should be accepted");
        DASSERTX(coid::token(result) == "C:/base/leaf"_T, "The result should be the absolute path itself");
    }

    // --- the two paths are joined with the platform separator when the base holds none ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a"_T, "b"_T, result, false, false);

        coid::charstr expected = "a";
        expected << sep << "b";

        DASSERTX(ok, "Joining two relative paths should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The platform separator should join the two paths");
    }

    // --- a base that already ends with a separator does not get a second one ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/"_T, "b"_T, result, false, false);

        DASSERTX(ok, "Joining onto a base ending with a separator should succeed");
        DASSERTX(coid::token(result) == "a/b"_T, "The separator already there should be the only one");
    }

    // --- a separator sitting inside the base is not the one the join is written with: the appended
    // component carries no separator in front of it, so the OS default one is used ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "c"_T, result, false, false);

        coid::charstr expected = "a/b";
        expected << sep << "c";

        DASSERTX(ok, "Joining onto a base holding a separator should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The OS default separator should join the two paths");
    }

    // --- an empty side leaves the other one alone, with no stray separator ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal(""_T, "b"_T, result, false, false);

        DASSERTX(ok, "An empty base should succeed");
        DASSERTX(coid::token(result) == "b"_T, "An empty base must not produce a leading separator");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a"_T, ""_T, result, false, false);

        DASSERTX(ok, "An empty appended path should succeed");
        DASSERTX(coid::token(result) == "a"_T, "An empty appended path must not produce a trailing separator");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal(""_T, ""_T, result, true, false);

        DASSERTX(ok, "Two empty paths should succeed");
        DASSERTX(result.is_empty(), "Two empty paths should build an empty result");
    }

    // --- without make_compact nothing is resolved, the two paths are only joined ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b/c/"_T, "../d"_T, result, false, false);

        DASSERTX(ok, "Joining without compacting should succeed");
        DASSERTX(coid::token(result) == "a/b/c/../d"_T, "The parent segment should be left where it is, with no separator added");
    }

    // --- make_compact resolves the parent and current dir segments of both sides ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "../c"_T, result, true, false);

        DASSERTX(ok, "Compacting a parent segment should succeed");
        DASSERTX(coid::token(result) == "a/c"_T, "The parent segment should consume the last base component");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/./b"_T, "c"_T, result, true, false);

        DASSERTX(ok, "Compacting a current dir segment should succeed");
        DASSERTX(coid::token(result) == "a/b\\c"_T, "A current dir segment should be removed");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a"_T, "b/../c"_T, result, true, false);

        DASSERTX(ok, "Compacting the appended path should succeed");
        DASSERTX(coid::token(result) == "a/c"_T, "The appended path should be compacted as well");
    }

    // --- make_compact collapses separator runs ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a//b"_T, "c"_T, result, true, false);

        DASSERTX(ok, "Compacting a separator run should succeed");
        DASSERTX(coid::token(result) == "a/b\\c"_T, "A separator run should collapse into one");
    }

    // --- a trailing separator of the appended path marks a directory and is kept ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "c/"_T, result, false, false);

        coid::charstr expected = "a/b";
        expected << sep << "c/";

        DASSERTX(ok, "Joining a directory path should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The trailing separator of the appended path should be kept");
    }

    // --- the component left after the compacting carries the separator written in front of it, the
    // OS default one is not the join here, and the trailing separator is kept ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "c/../d/"_T, result, true, false);

        DASSERTX(ok, "Compacting a directory path should succeed");
        DASSERTX(coid::token(result) == "a/b/d/"_T, "The component should be joined with the separator written in front of it");
    }

    // --- an appended path that resolves to nothing leaves the base as it is ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "."_T, result, true, false);

        DASSERTX(ok, "A current dir segment on its own should succeed");
        DASSERTX(coid::token(result) == "a/b"_T, "A current dir segment must not leave a separator behind");
    }

    // --- a relative base may legitimately be climbed out of when keep_below is not set ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a"_T, "../b"_T, result, true, false);

        DASSERTX(ok, "Climbing out of a relative base should succeed");
        DASSERTX(coid::token(result) == "b"_T, "The parent segment should consume the only base component");
    }

    // --- climbing above the root of an absolute base is an error ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/a"_T, "../.."_T, result, true, false);

        DASSERTX(!ok, "Climbing above the root of an absolute base should fail");
    }

    // --- keep_below accepts what stays below the base ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "c/d"_T, result, false, true);

        DASSERTX(ok, "A path staying below the base should be accepted");
        DASSERTX(coid::token(result) == "a/b\\c/d"_T, "The result should be the joined path");
    }

    // --- keep_below rejects what ends up outside the base ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "../c"_T, result, false, true);

        DASSERTX(!ok, "A path ending up outside the base should be rejected");
    }

    // --- only the result counts, a path that climbs out and back in is accepted ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "../b/c"_T, result, false, true);

        DASSERTX(ok, "A path leading back below the base should be accepted");
        DASSERTX(coid::token(result) == "a/b/c"_T, "The result should be the compacted path below the base");
    }

    // --- keep_below implies compacting, the result cannot be tested against the base otherwise ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "c/../d"_T, result, false, true);

        DASSERTX(ok, "A path that never escapes its own scope should be accepted");
        DASSERTX(coid::token(result) == "a/b/d"_T, "keep_below should compact even when make_compact is not set");
    }

    // --- the result is untouched on fail ---
    {
        coid::charstr result = "untouched";
        const bool ok = directory::build_path_internal("a/b"_T, "../c"_T, result, false, true);

        DASSERTX(!ok, "The call should fail");
        DASSERTX(coid::token(result) == "untouched"_T, "A failed call must leave the result untouched");
    }

    // --- to_separator rewrites the separators of the result ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a/b"_T, "c/d"_T, result, true, false, '\\');

        DASSERTX(ok, "Compacting with a target separator should succeed");
        DASSERTX(coid::token(result) == "a\\b\\c\\d"_T, "The separators of the result should be rewritten");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("a\\b"_T, "c"_T, result, true, false, '/');

        DASSERTX(ok, "Normalizing to forward slashes should succeed");
        DASSERTX(coid::token(result) == "a/b/c"_T, "Mixed separators should be normalized to the target one");
    }

    // --- the result may alias the buffer the base path points into ---
    {
        coid::charstr path = "a/b";
        const bool ok = directory::build_path_internal(path, "c"_T, path, false, false);

        DASSERTX(ok, "Building into the base buffer should succeed");
        DASSERTX(coid::token(path) == "a/b\\c"_T, "The result should be built even when it aliases the base");
    }

    // --- the result may alias the buffer the appended path points into ---
    {
        coid::charstr path = "c/d";
        const bool ok = directory::build_path_internal("a/b"_T, path, path, false, false);

        DASSERTX(ok, "Building into the appended path buffer should succeed");
        DASSERTX(coid::token(path) == "a/b\\c/d"_T, "The result should be built even when it aliases the appended path");
    }

    // --- an absolute base takes an appended path the same way a relative one does ---
#ifdef SYSTYPE_WIN
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/a"_T, "b"_T, result, true, false);

        coid::charstr expected = "C:/a";
        expected << sep << "b";

        DASSERTX(ok, "Joining onto an absolute base should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The appended component should be joined with the OS default separator");
    }

    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("//server/share"_T, "a/b"_T, result, true, false);

        coid::charstr expected = "//server/share";
        expected << sep << "a/b";

        DASSERTX(ok, "Joining onto a unc base should succeed");
        DASSERTX(coid::token(result) == coid::token(expected), "The unc root should be kept and the path joined to it");
    }

    // --- to_separator rewrites the root of the base along with the rest ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("C:/a"_T, "b"_T, result, true, false, '\\');

        DASSERTX(ok, "Joining onto an absolute base with a target separator should succeed");
        DASSERTX(coid::token(result) == "C:\\a\\b"_T, "The separators of the whole result should be rewritten");
    }
#else
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("/a"_T, "b"_T, result, true, false);

        DASSERTX(ok, "Joining onto an absolute base should succeed");
        DASSERTX(coid::token(result) == "/a/b"_T, "The appended component should be joined with the OS default separator");
    }

    // --- an absolute base that compacts down to its root takes the appended path onto the root ---
    {
        coid::charstr result;
        const bool ok = directory::build_path_internal("/a/.."_T, "b"_T, result, true, false);

        DASSERTX(ok, "Joining onto a base compacted down to its root should succeed");
        DASSERTX(coid::token(result) == "/b"_T, "The root separator should be the only one in front of the component");
    }
#endif // SYSTYPE_WIN
}

////////////////////////////////////////////////////////////////////////////////

void run_directory_tests()
{
    directory_tests::test_extract_path_component_internal();
    directory_tests::test_get_path_root_length_internal();
    directory_tests::test_do_append_compact_internal();
    directory_tests::test_is_same_path();
    directory_tests::test_directory_delete();
    directory_tests::test_directory_move();
    directory_tests::test_verify_path_syntax();
    directory_tests::test_extract_path_component();
    directory_tests::test_extract_path_component_root();
    directory_tests::test_extract_path_component_unc();
    directory_tests::test_build_path_internal();
    directory_tests::test_compact_path();
    directory_tests::test_create_compact_path();
    directory_tests::test_append_path();
    directory_tests::test_make_path();
}
