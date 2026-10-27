#include <comm/regex.h>
#include <comm/token.h>
#include <comm/str.h>
#include <comm/commassert.h>
#include <comm/commexception.h>
#include <comm/alloc/memtrack.h>
#include <comm/regex/regcomp.h>     // coid::Reinst, for counting its arrays in memtrack

#include <type_traits>
#include <vector>

/*
    Tests of coid::regex: compiling a pattern, and the three ways of running it - find() searches
    for the leftmost occurrence, match() has to cover the whole string, leading() has to start at
    the beginning of the string.

    Every result is a token pointing into the searched string, so most assertions check *where* a
    match is, not only what it reads: is_at() compares the result's position and length against
    the input. A token that reads right but points elsewhere would pass a plain string compare.

    "No match" is a null token from find() and leading(). match() reports a failure as an empty
    token, which is null or points at the unmatched rest of the string depending on where the
    machine stopped, so its tests check is_empty() only.

    Subexpression arrays follow the regex convention: sub[0] is the whole match and sub[n] is the
    n-th parenthesis, counted by its opening bracket.

    regex_tests_to_review() at the bottom holds tests of fixed defects that wait for review, the
    same way ref_tests_to_review() does.

    Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

using coid::regex;
using coid::token;
using coid::uints;

namespace {

/// @return true if found is the substring of text at the given offset and length
bool is_at(const token& found, const token& text, uints offset, uints length)
{
    return !found.is_null()
        && found.ptr() == text.ptr() + offset
        && found.len() == length;
}

#ifdef MEMTRACK_ENABLED
/// @return number of instruction arrays (one per compiled program) currently allocated
unsigned live_instruction_arrays()
{
    std::vector<coid::memtrack> entries(coid::memtrack_count() + 64);
    const unsigned count = coid::memtrack_list(entries.data(), unsigned(entries.size()), false);
    const unsigned hash = coid::type_info::get<coid::Reinst[]>().hash;
    for (unsigned i = 0; i < count; ++i) {
        if (entries[i].hash == hash)
            return entries[i].ncurallocs;
    }
    return 0;
}
#endif

/// @return true if compiling the pattern throws
bool compile_throws(const char* pattern)
{
    try {
        regex r(pattern);
    }
    catch (const coid::exception&) {
        return true;
    }
    return false;
}

void regex_compile_tests()
{
    // malformed patterns throw
    {
        RASSERT(compile_throws("(a"));          // unmatched left paren
        RASSERT(compile_throws("a)"));          // unmatched right paren
        RASSERT(compile_throws("()"));          // group without operand
        RASSERT(compile_throws("[a"));          // unterminated class
        RASSERT(compile_throws("[-a]"));        // range without a start
        RASSERT(compile_throws("[a-]"));        // range without an end
        RASSERT(compile_throws("*a"));          // operator without operand
        RASSERT(compile_throws("+"));
        RASSERT(compile_throws("?"));
        RASSERT(compile_throws("a|"));          // alternative without operand
        RASSERT(compile_throws("|a"));
        RASSERT(compile_throws("a||b"));
    }

    // well-formed patterns compile, including a repeated closure
    {
        RASSERT(!compile_throws("a"));
        RASSERT(!compile_throws("(a|b)*c+d?"));
        RASSERT(!compile_throws("[a-z0-9_]+"));
        RASSERT(!compile_throws("[^\\]]"));
        RASSERT(!compile_throws("a**"));
    }

    // one regex runs any number of times with the same result
    {
        regex r("[0-9]");
        token text = "x9y8";
        RASSERT(is_at(r.find(text), text, 1, 1));
        RASSERT(is_at(r.find(text), text, 1, 1));
        RASSERT(is_at(r.find(text.ptr() + 2), text, 3, 1));
    }

    // a compile that throws leaves the thread's compiler ready for the next compile
    {
        try {
            regex r("(a");
        }
        catch (const coid::exception&) {
        }

        try {
            regex r("a)");
        }
        catch (const coid::exception&) {
        }

        token text = "abc";
        RASSERT(is_at(regex("b").find(text), text, 1, 1));
        RASSERT(is_at(regex("(b|c)+").find(text), text, 1, 2));
    }

    // the instructions are stored in one array reserved for the worst case of two per pattern
    // byte; patterns made only of two-instruction elements compile and run
    {
        token text = "ac";
        RASSERT(is_at(regex("a?b?c?").match(text), text, 0, 2));
        token text1 = "a";
        RASSERT(is_at(regex("a??").match(text1), text1, 0, 1));
        RASSERT(is_at(regex("a|b|c").find(text), text, 0, 1));
        RASSERT(is_at(regex("((((a))))").find(text), text, 0, 1));
        RASSERT(is_at(regex("(a|b)?(c|d)?").match(text), text, 0, 2));
        RASSERT(is_at(regex("a?|c?").find(text), text, 0, 1));
        RASSERT(is_at(regex("[a][b]?[c]").match(text), text, 0, 2));

        coid::charstr many;
        for (int i = 0; i < 200; ++i)
            many << "a?";
        token text2 = "aaaa";
        RASSERT(is_at(regex(many).match(text2), text2, 0, 4));

        token text3 = "x(|)?*+.y";
        RASSERT(is_at(regex("(|)?*+.", true).find(text3), text3, 1, 7));

        RASSERT(compile_throws("((((a"));
        RASSERT(compile_throws("a))))"));
    }
}

void regex_find_tests()
{
    // alternation: the leftmost occurrence wins, whichever alternative it is
    {
        token text = "acb";
        RASSERT(regex("a|b").find(text) == "a");
        RASSERT(is_at(regex("a|b").find(text), text, 0, 1));

        token text2 = "xcb";
        RASSERT(is_at(regex("a|b").find(text2), text2, 2, 1));
    }

    // no occurrence gives a null token
    {
        RASSERT(regex("z").find("abc").is_null());
        RASSERT(regex("b").find("aaa").is_null());
        RASSERT(regex("b").find("").is_null());
    }

    // a match that is the whole string
    {
        token text = "a";
        RASSERT(is_at(regex("a").find(text), text, 0, 1));
    }

    // closures are greedy: the match extends as far as it can
    {
        token text = "baaa";
        RASSERT(is_at(regex("a+").find(text), text, 1, 3));

        token text2 = "acb3256825fa141423x123";
        RASSERT(regex("[0-9]+x").find(text2) == "141423x");
        RASSERT(is_at(regex("[0-9]+x").find(text2), text2, 12, 7));
    }

    // leftmost beats longest: a* matches the empty string at offset 0 before the a's
    {
        token text = "baaa";
        RASSERT(is_at(regex("a*").find(text), text, 0, 0));
    }

    // among matches at the same position the longest one wins, whatever the order of alternatives
    {
        token text = "ab";
        RASSERT(is_at(regex("a|ab").find(text), text, 0, 2));
        RASSERT(is_at(regex("ab|a").find(text), text, 0, 2));
    }

    // ? makes the preceding element optional
    {
        token text = "xacx";
        RASSERT(is_at(regex("ab?c").find(text), text, 1, 2));

        token text2 = "xabcx";
        RASSERT(is_at(regex("ab?c").find(text2), text2, 1, 3));
    }

    // . matches one character, and there is none past the end of the string
    {
        token text = "xabcx";
        RASSERT(is_at(regex("a.c").find(text), text, 1, 3));
        RASSERT(regex("a.").find("a").is_null());
    }

    // character classes, ranges and several ranges in one class
    {
        token text = "zzc";
        RASSERT(is_at(regex("[abc]").find(text), text, 2, 1));

        token text2 = "xxbcay";
        RASSERT(is_at(regex("[a-c]+").find(text2), text2, 2, 3));

        token text3 = "qazbyq";
        RASSERT(is_at(regex("[a-cx-z]+").find(text3), text3, 1, 4));
    }

    // a negated class matches anything outside it except a newline
    {
        token text = "a\nb";
        RASSERT(is_at(regex("[^a]").find(text), text, 2, 1));
        RASSERT(is_at(regex("[^x]+").find(text), text, 0, 1));
    }

    // an escaped metacharacter is a literal
    {
        token text = "a.b";
        RASSERT(is_at(regex("\\.").find(text), text, 1, 1));
        RASSERT(regex("\\.").find("ab").is_null());

        token text2 = "a**b";
        RASSERT(is_at(regex("\\*+").find(text2), text2, 1, 2));

        token text3 = "x-";
        RASSERT(is_at(regex("x[\\-a]").find(text3), text3, 0, 2));
    }

    // literal mode: the whole pattern is plain text
    {
        token text = "x.*y";
        RASSERT(is_at(regex(".*", true).find(text), text, 1, 2));
        RASSERT(regex(".*", true).find("xy").is_null());
    }

    // star_match_newline: . matches a newline as well
    {
        token text = "a\nc";
        RASSERT(is_at(regex("a.c", false, true).find(text), text, 0, 3));
    }

    // . does not match a newline unless star_match_newline is set
    {
        RASSERT(regex("a.c").find("a\nc").is_null());

        token text = "ab\ncd";
        RASSERT(is_at(regex(".+").find(text), text, 0, 2));
        RASSERT(is_at(regex(".+", false, true).find(text), text, 0, 5));
    }

    // ^ matches at the start of the string and after every newline
    {
        token text = "ab\nbc";
        RASSERT(is_at(regex("^b").find(text), text, 3, 1));
        RASSERT(regex("^a").find("xa").is_null());

        token text2 = "abc";
        RASSERT(is_at(regex("^").find(text2), text2, 0, 0));
    }

    // $ matches at the end of the string and before every newline
    {
        token text = "ab\nbc";
        RASSERT(is_at(regex("b$").find(text), text, 1, 1));

        token text2 = "abc";
        RASSERT(is_at(regex("c$").find(text2), text2, 2, 1));
        RASSERT(is_at(regex("$").find(text2), text2, 3, 0));
        RASSERT(regex("a$").find(text2).is_null());
    }

    // an empty pattern matches the empty string at the start
    {
        token text = "abc";
        RASSERT(is_at(regex("").find(text), text, 0, 0));
        RASSERT(is_at(regex("").leading(text), text, 0, 0));
        RASSERT(regex("").match(text).is_empty());
    }
}

void regex_match_tests()
{
    // the whole string matches
    {
        token text = "abc";
        RASSERT(is_at(regex("abc").match(text), text, 0, 3));

        token text2 = "aaa";
        RASSERT(is_at(regex("a*").match(text2), text2, 0, 3));

        token text3 = "b";
        RASSERT(is_at(regex("a|b").match(text3), text3, 0, 1));
    }

    // a string that only starts with a match fails
    {
        RASSERT(regex("a|b").match("acb") == "");
        RASSERT(regex("a|b").match("acb").is_empty());
    }

    // a string shorter than the pattern fails
    {
        RASSERT(regex("ab").match("a").is_empty());
    }

    // match() fails when the match stops short of the end of the string, also while the machine
    // has a live state left
    {
        RASSERT(regex("a").match("ab").is_empty());
        RASSERT(regex("a*").match("aab").is_empty());
        RASSERT(regex("abc").match("abcd").is_empty());

        token text = "aab";
        RASSERT(is_at(regex("a*b").match(text), text, 0, 3));
    }
}

void regex_leading_tests()
{
    // a match at the start of the string, not necessarily all of it
    {
        token text = "acb";
        RASSERT(regex("a|b").leading(text) == "a");
        RASSERT(is_at(regex("a|b").leading(text), text, 0, 1));

        token text2 = "abx";
        RASSERT(is_at(regex("ab").leading(text2), text2, 0, 2));

        token text3 = "aaab";
        RASSERT(is_at(regex("a+").leading(text3), text3, 0, 3));
    }

    // an occurrence further on does not count
    {
        RASSERT(regex("b").leading("ab").is_null());
    }

    // an empty match at the start is a match
    {
        token text = "bbb";
        RASSERT(is_at(regex("a*").leading(text), text, 0, 0));
    }
}

void regex_submatch_tests()
{
    // each group reports its own part of the match
    {
        token text = "acb628-x325-141-423-123";
        token sub[4];
        RASSERT(regex("([0-9]+)-([0-9]+)-([0-9]+)").find(text, sub, 4) == "325-141-423");
        RASSERT(sub[1] == "325");
        RASSERT(sub[2] == "141");
        RASSERT(sub[3] == "423");
        RASSERT(is_at(sub[1], text, 8, 3));
        RASSERT(is_at(sub[2], text, 12, 3));
        RASSERT(is_at(sub[3], text, 16, 3));
    }

    // the groups of a match that starts after the beginning of the string
    {
        token text = "aXbXc";
        token sub[2];
        RASSERT(is_at(regex("X(.)X").find(text, sub, 2), text, 1, 3));
        RASSERT(is_at(sub[1], text, 2, 1));
    }

    // only the alternative that matched reports a group
    {
        token text = "xby";
        token sub[3];
        RASSERT(is_at(regex("(a)|(b)").find(text, sub, 3), text, 1, 1));
        RASSERT(sub[1].is_null());
        RASSERT(is_at(sub[2], text, 1, 1));
    }

    // an optional group that did not take part is null
    {
        token text = "ab";
        token sub[3];
        RASSERT(is_at(regex("(a)(c)?b").find(text, sub, 3), text, 0, 2));
        RASSERT(is_at(sub[1], text, 0, 1));
        RASSERT(sub[2].is_null());
    }

    // a group under a closure reports its last iteration
    {
        token text = "abab";
        token sub[2];
        RASSERT(is_at(regex("(ab)*").match(text, sub, 2), text, 0, 4));
        RASSERT(is_at(sub[1], text, 2, 2));
    }

    // nested groups are numbered by their opening bracket
    {
        token text = "abcabc";
        token sub[3];
        RASSERT(is_at(regex("(a(b)c)+").match(text, sub, 3), text, 0, 6));
        RASSERT(is_at(sub[1], text, 3, 3));
        RASSERT(is_at(sub[2], text, 4, 1));
    }

    // a greedy group inside match()
    {
        token text = "aab";
        token sub[2];
        RASSERT(is_at(regex("(a*)b").match(text, sub, 2), text, 0, 3));
        RASSERT(is_at(sub[1], text, 0, 2));
    }

    // a failed search clears what an earlier search left in the array
    {
        token text = "ab";
        token sub[2];
        RASSERT(!regex("(b)").find(text, sub, 2).is_null());
        RASSERT(is_at(sub[1], text, 1, 1));
        RASSERT(regex("(x)").find(text, sub, 2).is_null());
        RASSERT(sub[1].is_null());
    }

    // sub[0] holds the whole match
    {
        token text = "x12-34y";
        token sub[3];
        RASSERT(is_at(regex("([0-9]+)-([0-9]+)").find(text, sub, 3), text, 1, 5));
        RASSERT(is_at(sub[0], text, 1, 5));

        token text2 = "aab";
        token sub2[2];
        RASSERT(is_at(regex("(a*)b").match(text2, sub2, 2), text2, 0, 3));
        RASSERT(is_at(sub2[0], text2, 0, 3));

        token text3 = "abc";
        token sub3[1];
        RASSERT(is_at(regex("c").find(text3, sub3, 1), text3, 2, 1));
        RASSERT(is_at(sub3[0], text3, 2, 1));
    }

    // groups beyond the caller's sub array - including no array at all - are matched but not
    // recorded
    {
        token text = "xa";
        RASSERT(is_at(regex("(a)").find(text), text, 1, 1));
        RASSERT(is_at(regex("((a))").find(text), text, 1, 1));

        token sub[1];
        RASSERT(is_at(regex("(a)").find(text, sub, 1), text, 1, 1));
        RASSERT(is_at(sub[0], text, 1, 1));

        token sub2[2];
        RASSERT(is_at(regex("(x)(a)").find(text, sub2, 2), text, 0, 2));
        RASSERT(is_at(sub2[1], text, 0, 1));

        token text2 = "xababab";
        RASSERT(is_at(regex("(ab)+").find(text2), text2, 1, 6));
        RASSERT(is_at(regex("x(a|b)*").find(text2), text2, 0, 7));
        RASSERT(is_at(regex("x(a|b)*").match(text2), text2, 0, 7));
    }
}

void regex_icase_tests()
{
    // a plain character matches either case, written in either case
    {
        token text = "ABC";
        RASSERT(is_at(regex("b", false, false, true).find(text), text, 1, 1));

        token text2 = "abc";
        RASSERT(is_at(regex("B", false, false, true).find(text2), text2, 1, 1));

        token text3 = "xAbx";
        RASSERT(is_at(regex("aB", false, false, true).find(text3), text3, 1, 2));
    }

    // a range matches either case
    {
        token text = "xBx";
        RASSERT(is_at(regex("[a-c]", false, false, true).find(text), text, 1, 1));

        token text2 = "xAy";
        RASSERT(is_at(regex("[A-C]", false, false, true).find(text2), text2, 1, 1));

        token text3 = "xay";
        RASSERT(is_at(regex("[A-C]", false, false, true).find(text3), text3, 1, 1));
    }

    // without icase the case matters
    {
        RASSERT(regex("b").find("ABC").is_null());
        RASSERT(regex("[a-c]").find("xBx").is_null());
    }

    // with icase, a single character in a class matches either case, like a range does
    {
        token text = "xBx";
        RASSERT(is_at(regex("[b]", false, false, true).find(text), text, 1, 1));

        token text2 = "zBz";
        RASSERT(is_at(regex("[ab]", false, false, true).find(text2), text2, 1, 1));

        token text3 = "zbz";
        RASSERT(is_at(regex("[B]", false, false, true).find(text3), text3, 1, 1));

        RASSERT(regex("[b]").find("xBx").is_null());
    }

    // with icase, a class with non-ASCII characters compiles and matches; only ASCII letters
    // are folded (\xc3\xa1 = U+00E1, \xc4\x8d = U+010D, \xc5\xbe = U+017E)
    {
        token text = "x\xc4\x8d";
        RASSERT(is_at(regex("[\xc3\xa1-\xc5\xbe]", false, false, true).find(text), text, 1, 2));
        RASSERT(is_at(regex("[\xc5\xbe\xc4\x8d]", false, false, true).find(text), text, 1, 2));
    }
}

void regex_utf8_tests()
{
    // patterns and strings are UTF-8, and a multibyte character is one character:
    // \xc3\xa1 = U+00E1 a-acute, \xc4\x8d = U+010D c-caron, \xc5\xa1 = U+0161 s-caron,
    // \xc5\xbe = U+017E z-caron

    // a multibyte literal
    {
        token text = "a\xc5\xbe";
        RASSERT(is_at(regex("\xc5\xbe").find(text), text, 1, 2));
    }

    // a multibyte character in a class
    {
        token text = "\xc5\xbe\xc3\xa1";
        RASSERT(is_at(regex("[\xc3\xa1]").find(text), text, 2, 2));
    }

    // a range of multibyte characters
    {
        token text = "x\xc4\x8d\xc5\xa1y";
        RASSERT(is_at(regex("[\xc3\xa1-\xc5\xbe]+").find(text), text, 1, 4));
    }

    // . takes a whole multibyte character
    {
        token text = "\xc5\xbe";
        RASSERT(is_at(regex(".").find(text), text, 0, 2));

        token text2 = "x\xc5\xbey";
        RASSERT(is_at(regex("x.y").find(text2), text2, 0, 4));
    }
}

void regex_ownership_tests()
{
    // a regex owns its program: it moves, and it does not copy
    {
        static_assert(!std::is_copy_constructible_v<regex>);
        static_assert(!std::is_copy_assignable_v<regex>);
        static_assert(std::is_nothrow_move_constructible_v<regex>);
        static_assert(std::is_nothrow_move_assignable_v<regex>);

        token text = "abbc";

        regex original("b+");
        regex moved(static_cast<regex&&>(original));
        RASSERT(is_at(moved.find(text), text, 1, 2));
        RASSERT(original.find(text).is_empty());        // moved-from: no program, like regex()

        regex assigned("x");
        assigned = static_cast<regex&&>(moved);
        RASSERT(is_at(assigned.find(text), text, 1, 2));
        RASSERT(moved.find(text).is_empty());

        regex& self = assigned;
        assigned = static_cast<regex&&>(self);
        RASSERT(is_at(assigned.find(text), text, 1, 2));
    }

    // compile() replaces the program; a malformed pattern throws and keeps the current one
    {
        token text = "abc";
        regex r("b");
        r.compile("c", false, false, false);
        RASSERT(is_at(r.find(text), text, 2, 1));

        bool threw = false;
        try {
            r.compile("(a", false, false, false);
        }
        catch (const coid::exception&) {
            threw = true;
        }
        RASSERT(threw);
        RASSERT(is_at(r.find(text), text, 2, 1));
    }

#ifdef MEMTRACK_ENABLED
    // a destroyed regex frees its program, also when the compile throws or a compile replaces it
    {
        const bool was_enabled = coid::memtrack_enable(true);
        const unsigned before = live_instruction_arrays();

        for (int i = 0; i < 100; ++i) {
            regex r("(a|b)*c");
            r.compile("[x-z]+", false, false, false);
            regex moved(static_cast<regex&&>(r));
            RASSERT(compile_throws("(a"));
        }

        RASSERT(live_instruction_arrays() == before);
        coid::memtrack_enable(was_enabled);
    }
#endif
}

/// Tests for defects that have just been fixed and are waiting to be reviewed. Once a fix is
/// signed off its block moves up into the suite it belongs to and this function shrinks back to
/// empty.
void regex_tests_to_review()
{
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
void regex_tests()
{
    regex_compile_tests();
    regex_find_tests();
    regex_match_tests();
    regex_leading_tests();
    regex_submatch_tests();
    regex_icase_tests();
    regex_ownership_tests();
    regex_utf8_tests();
    regex_tests_to_review();
}
