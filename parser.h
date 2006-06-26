
#ifndef __COID_COMM_PARSER__HEADER_FILE__
#define __COID_COMM_PARSER__HEADER_FILE__

#include "tokenizer.h"

///Recursive-descent parser

//ultimately we want the parser to be able to bootstrap itself
// for this we need the parser grammar

//The parser uses a separate lexer to read tokens of characters.
//A separate lexer is used because some common tasks can be much more
//effectively performed by a lexer, rather than having it all integrated
//in the parser. It's still possible to use only the parser, though, by
//using a primitive lexer.

//The tokenizer class used as lexer would also process strings with
//escape sequences and comment blocks.

//Even that the lexer is separated from the parser, rules of lexer can be
//addressed from within the parser grammar when prefixed with % character.
//So for example, if the lexer contains definitions for identifer, number
//and string, these can be used in grammar rules like this:
//
//  vardef: 'var' %identifier ('=' value)? ';';
//  value: %number | %string | %identifier;
//
//String literals.
//Lexer normally returns token of input data, with characters that belong
//to the same group or a string token. However, a string literal specified
//within the parser grammar can be composed from characters from different
//groups. That would mean that the parser should partition the literal to
//chunks of characters from the same groups, and try to match them
//sequentially, while instructing the lexer not to skip whitespace.
//A more effective means of matchin a string literal would be to hand the
//lexer directly the string literal to match. This can be used even for
//matching regular expressions.

//
//String terminals are surrounded with single quotes:
//  keyword: 'if' | 'else';
//

//Scanner config
//escape - escape char, char mappings
//string - leading seq, trailing seq, escape
//block - leading seq, trailing seq, escape, nested blocks
/*
identifier: 'a..zA..Z_0..9';
escape:     escape { '\\', ['\\'->'\\', 'n'->'\n', '\n'->''] };
sqstring:   string { '\'', '\'', escape };
dqstring:   string { '"', '"', escape };
code:       block { '{', '}',, [code,comment] };
comment:    string { '//', ['\n','\r\n','\r'] };
comment:    block { '/*', '* /',, comment  };
*/

//Grammar for the parser grammar.
/*
//lexer rules: identifier sqstring dqstring code
start:      prolog? rules epilog?;

prolog:     %code;
epilog:     %code;

rules:      rule+;
rule:       nonterm ':' expr ';';
nonterm:    %identifier;
expr:       subexpr ('|' subexpr)*;
subexpr:    seq+ code?;
seq:        ext | literal | regexp | lexrule;
ext:        (encl | nonterm) ('*' | '+' | '?')?;
encl:       '(' expr ')';
expr:       nonterm | literal | regexp | lexrule;

literal:    %sqstring;
regexp:     %dqstring;
lexrule:    '%' %identifier;
code:       %code;
*/

////////////////////////////////////////////////////////////////////////////////
class parser
{
    tokenizer _tkz_lexer;

public:

    parser()
    {
        _tkz_lexer.add_to_group( "ignore", " \t\n\r" );
        _tkz_lexer.add_to_group( "identifier", 'a', 'z' );
        _tkz_lexer.add_to_group( "identifier", 'A', 'Z' );
        _tkz_lexer.add_to_group( "identifier", '0', '9' );
        _tkz_lexer.add_to_group( "identifier", "_" );

        _tkz_lexer.add_escape_pair( "escape", '\\', '\\' );

        _tkz_lexer.add_string( "comment", "//", "\n", '\\', "escape" );
        _tkz_lexer.add_string( "comment", "//", "\r\n", '\\', "escape" );
        _tkz_lexer.add_string( "comment", "//", "\r", '\\', "escape" );
        _tkz_lexer.add_block( "comment", "/*", "*/", '\\', "escape" );

        _tkz_lexer.add_block( "code", "{", "}", 0, 0, "code,comment" );
    }
};

#endif //__COID_COMM_PARSER__HEADER_FILE__
