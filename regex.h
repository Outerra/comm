#ifndef __REGEXP_H
#define __REGEXP_H

#include "commtypes.h"
#include "dynarray.h"

COID_NAMESPACE_BEGIN

struct regex_program;
struct token;

///Regular expression class
/**
    Uses multiple-state implementation of NFA.

    Syntax:
     metacharacters: .*+?[]()|\^$ must be escaped by \ when used as literals

    A charclass is a nonempty string s bracketed [s] (or [^s]); it matches any character
     in (or not in) s. A negated character class never matches newline. A substring a-b,
     with a and b in ascending order, stands for the inclusive range of characters between
     a  and b. In s, the metacharacters -, ], an initial ^, and the regular expression
     delimiter must be preceded by a \; other metacharacters have no special meaning and
     may appear unescaped.

    A . matches any character.

    A ^ matches the beginning of a line; $ matches the end of the line.

    The *+? operators match zero or more (*), one or more (+), zero or one (?), instances
    respectively of the preceding regular expression.

    A concatenated regular expression, e1e2, matches a match to e1 followed by a match to e2.

    An alternative regular expression, e0|e1, matches either a match to e0 or a match to e1.

    A match to any part of a regular expression extends as far as possible without preventing
    a match to the remainder of the regular expression.
**/
struct regex
{
    regex();
    regex(token rt,
        bool literal = false,
        bool star_match_newline = false);

    void compile(token rt, bool literal, bool star_match_newline);


    ///Match the whole string
    token match( token bol, dynarray<token>& sub ) const;
    token match( token bol ) const { dynarray<token> empty; return match(bol, empty); }

    ///Find occurrence of pattern
    token find( token bol, dynarray<token>& sub ) const;
    token find( token bol ) const { dynarray<token> empty; return find(bol, empty); }

    ///Match leading part 
    token leading( token bol, dynarray<token>& sub ) const;
    token leading( token bol ) const { dynarray<token> empty; return leading(bol, empty); }


private:

    regex_program* _prog;
};

COID_NAMESPACE_END

#endif
