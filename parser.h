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
 * Brano Kemen
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __COID_COMM_PARSER__HEADER_FILE__
#define __COID_COMM_PARSER__HEADER_FILE__

#include "lexer.h"

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
// a group - characters to include, optional names of groups to include when
//           scanning subsequent characters or a different character set
// escape - escape char, char mappings
// string - leading seq, trailing seq, escape
// block - leading seq, trailing seq, escape, nested blocks
/*
ignore:     ' \t\n\r';
identifier: 'a..zA..Z', 'a..zA..Z_0..9';
operator:   '->';
separator:  single ':;,{}[]';
escape:     escape '\\', ['\\'->'\\', 'n'->'\n', '\n'->''];
sqstring:   string '\'', '\'', escape;
dqstring:   string '"', '"', escape;
code:       block '{', '}',, [code,comment];
comment:    string '//', ['\n','\r\n','\r'];
comment:    block '/*', '*' '/',, comment;
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

///Specialization of the lexer class; used for lexical analysis of grammar specification
/// itself. This one lexer is used for both lexer and parser grammars.
class grammar_lexer : public lexer
{
public:
    grammar_lexer()
    {
        def_group( "ignore", " \t\n\r" );
        def_group( "identifier", "a..zA..Z", "a..zA..Z_0..9" );
        def_group( "operator", "->" );
        def_group( "separator", ":;,[]" );

        int ie = def_escape( "escape", '\\', 0 );
        def_escape_pair( ie, "\\", "\\" );
        def_escape_pair( ie, "n", "\n" );
        def_escape_pair( ie, "\n", 0 );

        def_string( "sqstring", "'", "'", "escape" );
        def_string( "dqstring", "\"", "\"", "escape" );

        def_string( ".comment", "//", "\n", "escape" );
        def_string( ".comment", "//", "\r\n", "escape" );
        def_string( ".comment", "//", "\r", "escape" );
        def_block( ".blkcomment", "/*", "*/", ".comment sqstring dqstring" );

        def_block( "code", "{", "}", "code sqstring dqstring .blkcomment .comment" );
    }
};

////////////////////////////////////////////////////////////////////////////////
class parser
{
    grammar_lexer _lexer;

public:

    parser()
    {
    }

protected:


    ///Grammar symbol representation
    /**
        Symbol struct instances can represent a non-terminal or a terminal symbol.
        The terminal symbol can be a string literal to match or a token type to read.
        Non-terminal is composed of sequence of non-terminals and/or terminals.
        There can exist multiple non-terminals with the same name, in which case they
        represent possible parallel paths to take.
    **/
    struct symbol
    {
        charstr name;                   ///< non-terminal name or literal string

        virtual bool match( lexer& lex )
        {
            //literal string
            return lex.match(name);
        }
    };

    ///
    struct symval
    {
        symbol* sym;                    ///< symbol matched
        charstr val;                    ///< value read, unless the symbol was a literal string
    };

    ///Nonterminal definition
    /**
        There may be multiple definitions of the same non-terminal symbol, these are
        treated as parallel. Optional inline subrules are first split into separate
        rules.
    **/
    struct rule : symbol
    {
        dynarray<symval> seq;

        virtual bool match( lexer& lex )
        {

        }
    };
};

#endif //__COID_COMM_PARSER__HEADER_FILE__
