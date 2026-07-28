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

void run_directory_tests()
{
    test_is_same_path();
    test_directory_delete();
    test_directory_move();
}
