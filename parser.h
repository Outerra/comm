
#include "tokenizer.h"

//parser uses the tokenizer class to read the input
//Parser uses tokens (objects of class token) provided by the tokenizer.
// Some tasks can be more effectively performed by the tokenizer (lexer),
// such as scanning for comment blocks and/or strings, in which case the
// tokenizer method was_string() returns true and last_string_delimiter()
// returns the string type. The parser should deal with strings as with
// tokens of unique token type (they are even returned as tokens by the
// tokenizer)


///Recursive-descent parser

//ultimately we want the parser to be able to bootstrap itself
// for this we need the parser grammar
//definitions for the lexer will be contained there too

//
// String terminals are surrounded with single quotes:
//      keyword: 'if' | 'else'
//
// All characters within a string terminal must belong to the same
// tokenizer group.                        