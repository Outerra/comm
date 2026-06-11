#include <comm/dir.h>
#include <comm/commassert.h>


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

void run_directory_tests() 
{
    test_is_same_path();
}