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
 * Portions created by the Initial Developer are Copyright (C) 2006,2007
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


#ifndef __COID_COMM_LEXER__HEADER_FILE__
#define __COID_COMM_LEXER__HEADER_FILE__

#include "namespace.h"
#include "token.h"
#include "tutf8.h"
#include "dynarray.h"
#include "hash/hashkeyset.h"
#include "hash/hashset.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
///Class used to partition input string or stream to groups of similar characters
/**
    A lexer object must be initially configured by specifying character groups
    and sequence delimiters. The lexer then outputs tokens of characters
    from the input, with all characters in the token belonging to the same group.
    Group 0 is used as the ignore group; tokens of characters belonging there are
    not returned by default, just skipped.

        It's possible to define keywords that are returned as a different entity
    when encountered. As a token of some group is being read, hash value is
    computed along the scanning. If any keywords are defined, additional
    check is performed to see whether the token read has been a keyword or not.
    The token is then returned as of type ID_KEYWORDS instead of the original
    group id.

        Lexer can also detect sequences of characters that have higher priority
    than a simple grouping. It also processes strings and blocks, that are
    enclosed in custom delimiter sequences.
    Sequences alter behavior of the lexer by temporarily changing the rules. If a
    simple sequence can be matched, it has higher priority than ordinary character
    grouping even if the grouping would match longer input.
    Sequences are mostly used in pairs (one leading sequence and multiple trailing
    ones) to constraint an area with different set of rules.

        With the string sequence, lexer interprets characters in between the leading
    and trailing sequences as single token, but also performs substitutions of escape
    character sequences. It then outputs the processed content as single token.

        With the block sequence, lexer enables specific rules according to the
    specification for the block rule. It allows recursive processing of nested blocks,
    strings or sequences and normal tokens. Block content can be output as normal
    series of tokens followed by an end-of-block token, when using next() method.
    Alternatively, next_as_block() method can be used to read the content as a single
    token after the leading block token has been received.
    Different sets of rules can be enabled inside the blocks, or even the block
    itself may be disabled within itself (non-nestable).

    Sequences, strings and blocks can be in enabled or disabled state. This is used
    mainly to temporarily enable certain sequences that would be otherwise grouped
    with other characters and provide undesired tokens. This is used for example to
    solve the problem when < and > characters in C++ are used both as template
    argument list delimiters and operators. Parser cares for enabling and disabling
    particular sequences when appropriate.
    
    Sequences, strings and blocks can also be in ignored state. In this case they are
    being processed normally, but after succesfull recognition they are discarded and
    lexer reads next token. 

    The initial enabled/disabled and ignored state for a rule can be specified by
    prefixing the name with either . (dot, for ignored rule) or ! (exclamation mark,
    for disabled rule). This can be used also in the nesting list of block rule, where
    it directly specifies the nesting of rules within the block.

    It's possible to bind the lexer to a token (a string) source or to a binstream
    source, in which case the lexer also performs caching.

    The method next() is used to read subsequent tokens of input data. The last token
    read can be pushed back, to be returned again next time.

    The lexer uses first group as the one containing ignored whitespace characters,
    unless you provide the next() method with different group id (or none) to ignore.
**/
class lexer
{
public:

    ///Called before throwing an error exception to preset the _errtext member
    //@param rules true if the error occured during parsing the lexer grammar
    //@return the method should return false to make lexer ignore the error, works only in limited cases
    virtual bool on_error_prefix( bool rules ) {
        if(!rules) {
            uint col;
            uint row = current_line(0, &col);

            _errtext << row << ":" << col << ": ";
        }
        return true;
    }

    ///Incremental token hasher used by the lexer
    struct token_hash
    {
        uint    hash;
        typedef token key_type;

        ///Compute hash of token
        uint operator() (const token& s) const
        {
            uint h = 0;
            const char* p = s.ptr();
            const char* pe = s.ptre();

            for( ; p<pe; ++p )
                h = (h ^ (uint)*p) + (h<<26) + (h>>6);
            return h;
        }

        ///Incremental hash value computation
        void inc_char( char c ) {
            hash = (hash ^ (uint)c) + (hash<<26) + (hash>>6);
        }

        void reset()    { hash = 0; }
    };

    enum { ID_KEYWORDS = 0x8000 };

    ///Lexer output token
    struct lextoken
    {
        token   tok;                    ///< value string, points to input string or to tokbuf
        int     id;                     ///< token id (group type or string type)
        int     state;                  ///< >0 leading, <0 trailing, =0 chunked
        charstr tokbuf;                 ///< buffer that keeps processed string in case it had to be altered (escape seq.replacements etc.)
        token_hash hash;                ///< hash value computed for tokens (but not for strings or blocks)

        bool operator == (int i) const  { return i == id; }
        bool operator == (const token& t) const { return tok == t; }
        bool operator == (char c) const { return tok == c; }

        bool operator != (int i) const  { return i != id; }
        bool operator != (const token& t) const { return tok != t; }
        bool operator != (char c) const { return tok != c; }

        operator token() const          { return tok; }


        typedef int lextoken::*unspecified_bool_type;
	    operator unspecified_bool_type() const {
		    return id == 0 ? 0 : &lextoken::id;
	    }

        //@return true if this is end-of-file token
        bool end() const                { return id == 0; }

        //@return true if this is leading token of specified sequence
        bool leading(int i) const       { return state>0 && id==i; }

        //@return true if this is trailing token of specified sequence
        bool trailing(int i) const      { return state<0 && id==i; }

        ///Swap or assign token to the destination string
        void swap_to_string( charstr& buf )
        {
            if( !tokbuf.is_empty() )
                buf.swap(tokbuf);
            else
                buf = tok;
        }

        ///Swap content 
        token swap_to_token_or_string( charstr& dstr )
        {
            if( !tokbuf.is_empty() )
                dstr.swap(tokbuf);
            return tok;
        }

        token value() const             { return tok; }

        void upd_hash( const char* p )
        {
            hash.inc_char(*p);
        }

        void reset()
        {
            tokbuf.reset();
            hash.reset();
        }
    };

    ///Lexer exception
    struct lexception
    {
        int code;
        charstr text;

        lexception(int e, const charstr& text) : code(e), text(text)
        {}

        /// Error codes
        enum {
            //ERR_CHAR_OUT_OF_RANGE       = 1,
            ERR_ILL_FORMED_RANGE        = 2,
            ERR_ENTITY_EXISTS           = 3,
            ERR_ENTITY_DOESNT_EXIST     = 4,
            ERR_ENTITY_BAD_TYPE         = 5,
            ERR_DIFFERENT_ENTITY_EXISTS = 6,
            ERR_STRING_TERMINATED_EARLY = 7,
            ERR_UNRECOGNIZED_ESCAPE_SEQ = 8,
            ERR_BLOCK_TERMINATED_EARLY  = 9,
            ERR_EXTERNAL_ERROR          = 10,       ///< the error was set from outside
            ERR_KEYWORD_ALREADY_DEFINED = 11,
            ERR_INVALID_RULE_ID         = 12,
        };
    };




    ///Constructor
    //@param utf8 enable or disable utf8 mode
    lexer( bool utf8 = true )
    {
        _bin = 0;
        _utf8 = utf8;
        _ntrails = 0;

        reset();

        _abmap.need_new(256);
        for( uint i=0; i<_abmap.size(); ++i )
            _abmap[i] = GROUP_UNASSIGNED;
    }

    ///Enable or disable utf8 mode
    void set_utf8( bool set )
    {
        _utf8 = set;
    }


    ///Bind input binstream used to read input data
    opcd bind( binstream& bin )
    {
        _bin = 0;
        reset();

        _bin = &bin;
        return 0;
    }

    ///Bind input string to read from
    opcd bind( const token& tok )
    {
        _bin = 0;
        reset();

        _tok = tok;
        _last.tok.set(_tok.ptr(), 0);
        _lines_processed = _lines_last = _tok.ptr();
        return 0;
    }


    //@return true if the lexer is set up to interpret input as utf-8 characters
    bool is_utf8() const {
        return _abmap.size() == BASIC_UTF8_CHARS;
    }

    ///Reset lexer state (does not discard the rules)
    opcd reset()
    {
        if(_bin)
            _bin->reset_read();
        _tok.set_null();
        _binbuf.reset();
        _last.id = 0;
        _last.state = 0;
        _last.tok.set_null();
        _last_string = -1;
        _err = 0;
        _errtext.reset();
        _pushback = 0;
        *_stack.need(1) = &_root;

        _lines = 0;
        _lines_processed = _lines_last = 0;
        _lines_oldchar = 0;

        return 0;
    }

    ///Create new group named \a name with characters from \a set. Clump of continuous characters
    /// from the group is returned as one token.
    //@return group id or 0 on error, error id is stored in the _err variable
    //@param name group name
    //@param set characters to include in the group, recognizes ranges when .. is found
    //@param trailset optional character set to match after the first character from the
    /// set was found. This can be used to allow different characters after the first
    /// letter matched
    int def_group( const token& name, const token& set, const token& trailset = token::empty() )
    {
        uint g = (uint)_grpary.size();

        if( _entmap.find_value(name) )
            __throw_entity_exists(name);

        group_rule* gr = new group_rule( name, (ushort)g, false );
        _entmap.insert_value(gr);

        uchar msk = (uchar)g;
        if( !trailset.is_empty() )
            msk |= fGROUP_TRAILSET;

        if( !process_set( set, msk, &lexer::fn_group ) )  return 0;
        if( !trailset.is_empty() )
        {
            gr->bitmap = _ntrails;
            _trail.needc( _abmap.size() );
            if( !process_set( trailset, 1<<_ntrails++, &lexer::fn_trail ) )  return 0;
        }

        *_grpary.add() = gr;
        return g+1;
    }

    ///Create new group named \a name with characters from \a set that will be returned
    /// as single character tokens
    //@return group id or 0 on error, error id is stored in the _err variable
    //@param name group name
    //@param set characters to include in the group, recognizes ranges when .. is found
    int def_group_single( const token& name, const token& set )
    {
        uint g = (uint)_grpary.size();

        if( _entmap.find_value(name) )
            __throw_entity_exists(name);

        group_rule* gr = new group_rule( name, (ushort)g, true );
        _entmap.insert_value(gr);

        if( !process_set( set, (uchar)g | fGROUP_SINGLE, &lexer::fn_group ) )  return 0;

        *_grpary.add() = gr;
        return g+1;
    }

    ///Define character sequence (keyword) that should be returned as a separate token type.
    ///If the characters of specific keyword all fit in the same character group, they are
    /// implemented as lookups into the keyword hash table after the scanner computes
    /// the hash value for the token it processed.
    ///Otherwise (when they are heterogenous) they are set up as sequences.
    //@param kwd keyword to add
    //@return ID_KEYWORDS if the keyword consists of homogenous characters from
    /// one character group, or id of a sequence created, or 0 if already defined.
    int def_keyword( const token& kwd )
    {
        if( !homogenous(kwd) )
            return def_sequence( "keywords", kwd );

        if( _kwds.add(kwd) ) {
            _abmap[(uchar)kwd.first_char()] |= fGROUP_KEYWORDS;
            return ID_KEYWORDS;
        }

        _err = lexception::ERR_KEYWORD_ALREADY_DEFINED;
        on_error_prefix(true);

        _errtext << "keyword '" << kwd << "' already exists";

        throw lexception(_err, _errtext);
    }

    ///Escape sequence processor function prototype.
    ///Used to consume input after escape character and to append translated characters to dst
    //@param src source characters to translate, the token should be advanced by the consumed amount
    //@param dst destination buffer to append the translated characters to
    typedef bool (*fn_replace_esc_seq)( token& src, charstr& dst );

    ///Define an escape rule. Escape replacement pairs are added subsequently via def_escape_pair()
    //@return the escape rule id, or -1 on error
    //@param name the escape rule name
    //@param escapechar the escape character used to prefix the sequences
    //@param fn_replace pointer to function that should perform the replacement
    int def_escape( const token& name, char escapechar, fn_replace_esc_seq fn_replace = 0 )
    {
        uint g = (uint)_escary.size();

        //only one escape rule of given name
        if( _entmap.find_value(name) )
            __throw_entity_exists(name);

        escape_rule* er = new escape_rule( name, (ushort)g );
        _entmap.insert_value(er);

        er->esc = escapechar;
        er->replfn = fn_replace;

        _abmap[(uchar)escapechar] |= fGROUP_ESCAPE;

        *_escary.add() = er;
        return g+1;
    }

    ///Define the escape string mappings.
    //@return true if successful
    //@param escrule id of the escape rule
    //@param code source sequence that if found directly after the escape character, will be replaced
    //@param replacewith the replacement string
    //@note To define rule that replaces escaped \n, specify \n for code and an empty token for
    /// the replacewith param. To specify a sequence that inserts \0 character(s), the replacewith
    /// token must be explicitly set to include it, like token("\0", 1).
    bool def_escape_pair( int escrule, const token& code, const token& replacewith )
    {
        --escrule;
        if( escrule < 0  &&  escrule >= (int)_escary.size() )  return false;

        //add escape pairs, longest codes first
        escpair* ep = _escary[escrule]->pairs.add_sortT(code);
        ep->assign( code, replacewith );

        return true;
    }


    ///Create new string sequence detector named \a name, using specified leading and trailing sequences.
    ///If the leading sequence was already defined for another string, only the trailing sequence is
    /// inserted into its trailing set, and the escape definition is ignored.
    //@return string id (a negative number) or 0 on error, error id is stored in the _err variable
    //@param name string rule name. Name prefixed with . (dot) makes the block content ignored in global scope.
    //@param leading the leading string delimiter
    //@param trailing the trailing string delimiter. An empty string means end of file.
    //@param escape name of the escape rule to use for processing of escape sequences within strings
    int def_string( token name, const token& leading, const token& trailing, const token& escape )
    {
        uint g = (uint)_stbary.size();

        bool enb, ign;
        get_flags(name, &ign, &enb);

        entity* const* ens = _entmap.find_value(name);
        if( ens && (*ens)->type != entity::STRING )
            __throw_different_exists(name);

        if( ens && ((string_rule*)*ens)->leading == leading ) {
            //reuse, only the trailing mark is different
            g = add_trailing( ((string_rule*)*ens), trailing );
            return -1-g;
        }

        string_rule* sr = new string_rule( name, (ushort)g );
        _entmap.insert_value(sr);

        _root.ignore(sr->id, ign);
        _root.enable(sr->id, enb);

        if( !escape.is_empty() )
        {
            entity* const* en = _entmap.find_value(escape);
            sr->escrule = en ? reinterpret_cast<escape_rule*>(*en) : 0;
            if( !sr->escrule )
                __throw_doesnt_exist(escape);

            if( sr->escrule->type != entity::ESCAPE ) {
                _err = lexception::ERR_ENTITY_BAD_TYPE;
                on_error_prefix(true);

                _errtext << "bad rule type '" << escape << "'; expected escape rule";

                throw lexception(_err, _errtext);
            }
        }

        sr->leading = leading;
        add_trailing( sr, trailing );

        add_sequence(sr);
        return -1-g;
    }


    ///Create new block sequence detector named \a name, using specified leading and trailing sequences.
    //@return block id (a negative number) or 0 on error, error id is stored in the _err variable
    //@param name block rule name. Name prefixed with . (dot) makes the block content ignored in global scope.
    //@param leading the leading block delimiter
    //@param trailing the trailing block delimiter
    //@param nested names of recognized nested blocks and strings to look and account for (separated with
    /// spaces). An empty list disables any nested blocks and strings.
    //@note The rule names (either in the name or the nested params) can start with '.' (dot) character,
    /// in which case the rule is set to ignored state either within global scope (if placed before the
    /// rule name) or within the rule scope (if placed before rule names in nested list).
    int def_block( token name, const token& leading, const token& trailing, token nested )
    {
        uint g = (uint)_stbary.size();

        bool enb, ign;
        get_flags(name, &ign, &enb);

        entity* const* ens = _entmap.find_value(name);
        if( ens && (*ens)->type != entity::BLOCK )
            __throw_different_exists(name);

        if( ens && ((block_rule*)*ens)->leading == leading ) {
            //reuse, only trailing mark is different
            g = add_trailing( ((block_rule*)*ens), trailing );
            return -1-g;
        }

        block_rule* br = new block_rule( name, (ushort)g );
        _entmap.insert_value(br);

        _root.ignore(br->id, ign);
        _root.enable(br->id, enb);

        for(;;)
        {
            token ne = nested.cut_left(' ');
            if(ne.is_empty())  break;

            if(ne == '*') {
                //inherit all global rules
                br->stbenabled = _root.stbenabled;
                br->stbignored = _root.stbignored;
                continue;
            }

            bool ignn, enbn;
            get_flags( ne, &ignn, &enbn );

            if( ne.char_is_number(0) )
            {
                token net = ne;

                //special case for cross-linked blocks, id of future block rule instead of the name
                int rn = ne.touint_and_shift();
                if( ne.len() )
                    __throw_different_exists(net);

                br->enable(rn, enbn);
                br->ignore(rn, ignn);
            }
            else
            {
                enable_in_block( br, ne, enbn );
                ignore_in_block( br, ne, ignn );
            }
        }

        br->leading = leading;
        add_trailing( br, trailing );

        add_sequence(br);
        return -1-g;
    }


    ///Create new simple sequence detector named \a name.
    //@return block id (a negative number) or 0 on error, error id is stored in the _err variable
    //@param name block rule name. Name prefixed with . (dot) makes the block content ignored in global scope.
    //@param seq the sequence to detect, prior to basic grouping rules
    int def_sequence( token name, const token& seq )
    {
        uint g = (uint)_stbary.size();

        bool enb, ign;
        get_flags(name, &ign, &enb);

        entity* const* ens = _entmap.find_value(name);
        if( ens && (*ens)->type != entity::SEQUENCE )
            __throw_different_exists(name);

        sequence* se = new sequence( name, (ushort)g );
        _entmap.insert_value(se);

        _root.ignore(se->id, ign);
        _root.enable(se->id, enb);

        se->leading = seq;

        add_sequence(se);
        return -1-g;
    }

    ///Enable or disable specified sequence, string or block. Disabled construct is not detected in input.
    //@note for s/s/b with same name, this applies only to the specific one
    void enable( int seqid, bool en )
    {
        __assert_valid_sequence(seqid, 0);

        uint sid = -1 - seqid;
        const sequence* seq = _stbary[sid];

        (*_stack.last())->enable(seq->id, en);
    }

    ///Make sequence, string or block ignored or not. Ignored constructs are detected but skipped and not returned.
    //@note for s/s/b with same name, this applies only to the specific one
    void ignore( int seqid, bool ig )
    {
        __assert_valid_sequence(seqid, 0);

        uint sid = -1 - seqid;
        const sequence* seq = _stbary[sid];

        (*_stack.last())->ignore(seq->id, ig);
    }

    ///Enable/disable all entities with the same (common) name.
    void enable( token name, bool en )
    {
        enable_in_block( *_stack.last(), name, en );
    }

    ///Ignore/don't ignore all entities with the same name
    void ignore( token name, bool ig )
    {
        ignore_in_block( *_stack.last(), name, ig );
    }

    ///Return next token as if the string opening sequence has been already read.
    //@note this explicit request will read the string content even if the
    /// string is currently disabled
    const lextoken& next_as_string( int stringid )
    {
        __assert_valid_sequence(stringid, entity::STRING);

        uint sid = -1 - stringid;
        sequence* seq = _stbary[sid];

        DASSERT( seq->is_string() );

        uint off=0;
        return next_read_string( *(string_rule*)seq, off, true );
    }

    ///Return next token as if the block opening sequence has been already read.
    //@note this explicit request will read the block content even if the
    /// block is currently disabled
    const lextoken& next_as_block( int blockid )
    {
        __assert_valid_sequence(blockid, entity::BLOCK);

        uint sid = -1 - blockid;
        sequence* seq = _stbary[sid];

        DASSERT( seq->is_block() );

        uint off=0;
        return next_read_block( *(block_rule*)seq, off, true );
    }

    ///Return next token, appending it to the \a dst
    charstr& next_append( charstr& dst )
    {
        token t = next();
        if( !_last.tokbuf.is_empty() && dst.is_empty() )
            dst.takeover(_last.tokbuf);
        else
            dst.append(t);
        return dst;
    }

    ///Return current block as single token
    /** Finds the end of active block and returns the content as a single token.
        For the top-most, implicit block it processes input to the end. Enabled sequences
        are processed normally: ignored sequences are read but discarded, strings have
        their escape sequences replaced, etc.
    **/
    const lextoken& match_block( int blockid )
    {
        __assert_valid_sequence(blockid, entity::BLOCK);

        uint sid = -1 - blockid;
        sequence* seq = _stbary[sid];

        DASSERT( seq->is_block() );

        bool enb = enabled(*seq);
        if(!enb)
            enable(sid, true);

        next();
        if(!enb)
            enable(sid, false);

        if(_last.id == blockid) {
            uint off=0;
            return next_read_block( *(block_rule*)seq, off, true );
        }

        _err = lexception::ERR_EXTERNAL_ERROR;
        on_error_prefix(false);

        _errtext << "expected block: " << seq->name;
        throw lexception(_err, _errtext);
    }

    ///Return the next token from input.
    /**
        Returns token of characters identified by a lexer rule (group or sequence types).
        Tokens of rules that are currently set to ignored are skipped.
        Rules that are disabled in current scope are not considered.
        On error an empty token is returned and _err contains the error code.

        @param ignoregrp id of the group to ignore if found at the beginning, 0 for none.
        This is used mainly to omit the whitespace (group 1), or explicitly to not skip it.
    **/
    const lextoken& next( uint ignoregrp=1 )
    {
        //return last token if instructed
        if(_pushback) { _pushback = 0; return _last; }

        _last.reset();

        //skip characters from the ignored group
        if(ignoregrp) {
            _last.tok = scan_group( ignoregrp-1, true );
            if( _last.tok.ptr() == 0 )
                return set_end();
        }
        else if( _tok.len() == 0 )
        {
            //end of buffer
            // if there's a source binstream connected, try to suck more data from it
            if( !_bin || fetch_page(0, false) == 0 )
            {
                //there still may be block rule active that is terminatable by eof
                if( _stack.size()>1  &&  (*_stack.last())->trailing.last()->seq.is_empty() )
                {
                    const block_rule& br = **_stack.pop();

                    uint k = br.id;
                    _last.id = -1 - k;
                    _last.state = -(int)k;
                    _last_string = k;

                    _last.tok.set_empty();
                    return _last;
                }

                return set_end();
            }
        }

        uchar code = *_tok.ptr();
        ushort x = _abmap[code];        //get mask for the leading character

        //check if it's the trailing sequence
        int kt;
        uint off=0;

        if( (x & fSEQ_TRAILING)
            && _stack.size()>1
            && (kt = match_trail((*_stack.last())->trailing, off)) >= 0 )
        {
            const block_rule& br = **_stack.last();

            uint k = br.id;
            _last.id = -1 - k;
            _last.state = -(int)k;
            _last_string = k;

            _last.tok = _tok.cut_left_n( br.trailing[kt].seq.len() );
            _stack.pop();

            return _last;
        }

        if(x & xSEQ)
        {
            //this could be a leading string/block delimiter, if we can find it in the register
            const dynarray<sequence*>& dseq = _seqary[((x&xSEQ)>>rSEQ)-1];
            uint i, n = (uint)dseq.size();
            for( i=0; i<n; ++i )
                if( enabled(*dseq[i])  &&  follows(dseq[i]->leading) )  break;

            if(i<n)
            {
                sequence* seq = dseq[i];
                uint k = seq->id;
                _last.id = -1 - k;
                _last.state = k;
                _last_string = k;

                _last.tok = _tok.cut_left_n( seq->leading.len() );

                //this is a leading string or block delimiter
                uints off=0;

                if( seq->type == entity::BLOCK )
                {
                    if( !ignored(*seq) )
                    {
                        _stack.push( reinterpret_cast<block_rule*>(seq) );
                        return _last;
                    }

                    next_read_block( *(const block_rule*)seq, off, true );
                }
                else if( seq->type == entity::STRING )
                    next_read_string( *(const string_rule*)seq, off, true );

                if( ignored(*seq) )   //ignored rule
                    return next(ignoregrp);

                //if(!st)
                //    _last.tok.set_empty();  //there was an error reading the string or block

                return _last;
            }
        }

        _last.id = x & xGROUP;
        ++_last.id;

        _last.upd_hash( _tok.ptr() );

        //normal token, get all characters belonging to the same group, unless this is
        // a single-char group
        if( x & fGROUP_SINGLE )
        {
            if(_utf8) {
                //return whole utf8 characters
                _last.tok = _tok.cut_left_n( prefetch_utf8() );
            }
            else
                _last.tok = _tok.cut_left_n(1);
            return _last;
        }

        //consume remaining
        if( x & fGROUP_TRAILSET )
            _last.tok = scan_mask( 1<<_grpary[_last.id-1]->bitmap, false, 1 );
        else
            _last.tok = scan_group( _last.id-1, false, 1 );

        //check if it's a keyword
        if( (x & fGROUP_KEYWORDS)  &&  _kwds.valid( _last.hash.hash, _last.tok ) )
            _last.id = ID_KEYWORDS;

        return _last;
    }

    ///Try to match a raw string following in the input
    bool follows( const token& tok )
    {
        if( tok.len() > _tok.len() )
        {
            uints n = fetch_page(_tok.len(), false);
            if( n < tok.len() )  return false;
        }

        return _tok.begins_with(tok);
    }

    //@return true if next token matches literal string
    bool matches( const token& val )  { return next() == val; }

    //@return true if next token matches literal character
    bool matches( char c )            { return next() == c; }

    //@return true if next token belongs to the specified group/sequence.
    //@param grp group/sequence id
    //@param dst the token read
    bool matches( int grp, charstr& dst )
    {
        __assert_valid_rule(grp);

        next();

        if( _last != grp )
            return false;

        if( !_last.tokbuf.is_empty() )
            dst.takeover( _last.tokbuf );
        else
            dst = _last.tok;
        return true;
    }

    //@return true if next token belongs to the specified group/sequence.
    //@param grp group/sequence id
    bool matches( int grp )
    {
        __assert_valid_rule(grp);

        next();

        return _last == grp;
    }

    //@return true if the last token read belongs to the specified group/sequence.
    //@param grp group/sequence id
    //@param dst the token read
    bool matches_last( int grp, charstr& dst )
    {
        __assert_valid_rule(grp);

        if( grp != _last.id )
            return false;

        if( !_last.tokbuf.is_empty() )
            dst.takeover( _last.tokbuf );
        else
            dst = _last.tok;
        return true;
    }

    ///Match literal or else throw exception (struct lexception)
    void match( const token& val )
    {
        if(!matches(val)) {
            _err = lexception::ERR_EXTERNAL_ERROR;
            on_error_prefix(false);

            _errtext << "expected " << val;

            throw lexception(_err, _errtext);
        }
    }

    ///Match literal or else throw exception (struct lexception)
    void match( char c )
    {
        if(!matches(c)) {
            _err = lexception::ERR_EXTERNAL_ERROR;
            on_error_prefix(true);

            _errtext << "expected " << c;

            throw lexception(_err, _errtext);
        }
    }

    ///Match rule or else throw exception (struct lexception)
    void match( int grp, charstr& dst )
    {
        if(!matches(grp, dst)) {
            _err = lexception::ERR_EXTERNAL_ERROR;
            on_error_prefix(true);

            const entity& ent = get_entity(grp);
            _errtext << "expected a " << ent.entity_type() << " '" << ent.name << "'";

            throw lexception(_err, _errtext);
        }
    }

    ///Match rule or else throw exception (struct lexception)
    void match( int grp )
    {
        if(!matches(grp)) {
            _err = lexception::ERR_EXTERNAL_ERROR;
            on_error_prefix(true);

            const entity& ent = get_entity(grp);
            _errtext << "expected a " << ent.entity_type() << " '" << ent.name << "'";

            throw lexception(_err, _errtext);
        }
    }

    ///Try to match an optional literal, push back if not succeeded.
    bool match_optional( const token& val )
    {
        bool succ = (next() == val);
        if(!succ) 
            push_back();
        return succ;
    }

    ///Try to match an optional literal character, push back if not succeeded.
    bool match_optional( char c )
    {
        bool succ = (next() == c);
        if(!succ) 
            push_back();
        return succ;
    }

    ///Try to match an optional rule
    bool match_optional( int grp )
    {
        __assert_valid_rule(grp);

        bool succ = matches(grp);
        if(!succ)
            push_back();

        return succ;
    }

    ///Try to match an optional rule, setting matched value to \a dst
    bool match_optional( int grp, charstr& dst )
    {
        __assert_valid_rule(grp);

        bool succ = matches(grp, dst);
        if(!succ)
            push_back();

        return succ;
    }

    //@{
    ///Try to match one of the rules. The parameters can be either literals (strings or
    /// single characters) or rule identifiers.
    //@return true if succesfull; matched rule can be found by calling last(). Returns
    /// false if none were matched.
    //@note if nothing was matched, the lexer doesn't consume anything from the stream
    template<class T1, class T2>
    bool matches_either( T1 a, T2 b ) {
        return match_optional(a) || match_optional(b);
    }

    template<class T1, class T2, class T3>
    bool matches_either( T1 a, T2 b, T3 c ) {
        return match_optional(a) || match_optional(b) || match_optional(c);
    }

    template<class T1, class T2, class T3, class T4>
    bool matches_either( T1 a, T2 b, T3 c, T4 d ) {
        return match_optional(a) || match_optional(b) || match_optional(c)
            || match_optional(d);
    }
    //@}


    //@{
    ///Match one of the rules. The parameters can be either literals (strings or
    /// single characters) or rule identifiers.
    ///If no parameter is matched, throws exception (struct lexception).
    template<class T1, class T2>
    const lextoken& match_either( T1 a, T2 b )
    {
        if(match_optional(a) || match_optional(b))
            return _last;

        _err = lexception::ERR_EXTERNAL_ERROR;
        on_error_prefix(true);

        _errtext << "couldn't match either: ";
        rule_map<T1>::desc(a, *this, _errtext); _errtext << ", ";
        rule_map<T2>::desc(b, *this, _errtext);

        throw lexception(_err, _errtext);
    }

    template<class T1, class T2, class T3>
    const lextoken& match_either( T1 a, T2 b, T3 c )
    {
        if(match_optional(a) || match_optional(b) || match_optional(c))
            return _last;

        _err = lexception::ERR_EXTERNAL_ERROR;
        on_error_prefix(true);

        _errtext << "couldn't match either: ";
        rule_map<T1>::desc(a, *this, _errtext); _errtext << ", ";
        rule_map<T2>::desc(b, *this, _errtext); _errtext << ", ";
        rule_map<T3>::desc(c, *this, _errtext);

        throw lexception(_err, _errtext);
    }

    template<class T1, class T2, class T3, class T4>
    const lextoken& match_either( T1 a, T2 b, T3 c, T4 d )
    {
        if(match_optional(a) || match_optional(b) || match_optional(c)
            || match_optional(d))
            return _last;

        _err = lexception::ERR_EXTERNAL_ERROR;
        on_error_prefix(true);

        _errtext << "couldn't match either: ";
        rule_map<T1>::desc(a, *this, _errtext); _errtext << ", ";
        rule_map<T2>::desc(b, *this, _errtext); _errtext << ", ";
        rule_map<T3>::desc(c, *this, _errtext); _errtext << ", ";
        rule_map<T4>::desc(d, *this, _errtext);

        throw lexception(_err, _errtext);
    }
    //@}

    ///Push the last token back to be retrieved again by the next() method
    /// (and all the methods that use it, like the match_* methods etc.).
    void push_back() {
        _pushback = 1;
    }

    //@return true if whole input was consumed
    bool end() const                    { return _last.id == 0; }

    //@return last token read
    const lextoken& last() const        { return _last; }

    //@return current line number
    //@param text optional token, recieves current line text (may be incomplete)
    //@param col receives column number of current token
    uint current_line( token* text=0, uint* col=0 )
    {
        if( _tok.ptr() > _lines_processed )
            _lines += count_newlines( _lines_processed, _tok.ptr() );

        if(text) {
            uints n = _tok.count_notingroup("\r\n");
            const char* last = _lines_last > _binbuf.ptr()
                ? _lines_last
                : _binbuf.ptr();

            text->set( last, _tok.ptr()-last+n );
        }
        if(col)
            *col = uint(_last.tok.ptr() - _lines_last);

        return _lines+1;
    }

    ///Get error code
    int err( token* errstr ) const {
        if(errstr)
            *errstr = _errtext;
        return _err;
    }

    ///Return error text
    const charstr& err() const {
        return _errtext;
    }

    ///Prepare to throw an external lexer exception.
    ///This is used to obtain an exception with the same format as with an internal lexer exception.
    ///The line and column info relates to the last returned token. After using the return value to
    /// provide specific error text, call throw exception().
    //@return string object that can be used to fill specific info; the string is
    /// already prefilled with whatever the on_error_prefix() handler inserted into it
    //@note 
    charstr& prepare_exception()
    {
        _err = lexception::ERR_EXTERNAL_ERROR;
        _errtext.reset();
        on_error_prefix(false);

        return _errtext;
    }

    //@return lexception object to be thrown
    lexception exception() const {
        return lexception(_err, _errtext);
    }



    ///Test if whole token consists of characters from single group.
    //@return 0 if not, or else the character group it belongs to (>0)
    int homogenous( token t ) const
    {
        uchar code = t[0];
        ushort x = _abmap[code];        //get mask for the leading character

        int gid = x & xGROUP;
        ++gid;

        //normal token, get all characters belonging to the same group, unless this is
        // a single-char group
        uint n;
        if( x & fGROUP_SINGLE )
            n = _utf8  ?  prefetch_utf8(t)  :  1;
        else if( x & fGROUP_TRAILSET )
            n = (uint)count_inmask_nohash( t, 1<<_grpary[gid-1]->bitmap, 1 );
        else
            n = (uint)count_intable_nohash( t, gid-1, 1 );
        
        return n<t.len()  ?  0  :  gid;
    }

    ///Strip the leading and trailing characters belonging to the given group
    token& strip_group( token& tok, uint grp ) const
    {
        //only character groups
        DASSERT( grp > 0 );
        --grp;

        while( !tok.is_empty() )
        {
            if( grp != get_group( tok.first_char() ) )
                break;
            ++tok;
        }
        while( !tok.is_empty() )
        {
            if( grp != get_group( tok.last_char() ) )
                break;
            tok--;
        }
        return tok;
    }


    ///Synthesize proper escape sequences for given string
    //@param string id of the string type as returned by def_string
    //@param tok string to synthesize
    //@param dst destination storage where altered string is appended (if not altered it's not used)
    //@return true if the string has been altered
    bool synthesize_string( int string, token tok, charstr& dst ) const
    {
        __assert_valid_sequence(string, entity::STRING);

        uint sid = -1 - string;
        sequence* seq = _stbary[sid];

        if( !seq->is_string() )
            throw ersINVALID_PARAMS;

        const string_rule* sr = reinterpret_cast<string_rule*>(seq);
        if(!sr)  return false;

        return sr->escrule->synthesize_string(tok, dst);
    }


private:
    void __throw_entity_exists( const token& name )
    {
        _err = lexception::ERR_ENTITY_EXISTS;
        on_error_prefix(true);

        _errtext << "a rule with name '" << name << "' already exists";

        throw lexception(_err, _errtext);
    }

    void __throw_different_exists( const token& name )
    {
        _err = lexception::ERR_DIFFERENT_ENTITY_EXISTS;
        on_error_prefix(true);

        _errtext << "a different type of rule '" << name << "' already exists";

        throw lexception(_err, _errtext);
    }

    void __throw_doesnt_exist( const token& name )
    {
        _err = lexception::ERR_ENTITY_DOESNT_EXIST;
        on_error_prefix(true);

        _errtext << "specified rule '" << name << "' doesn't exist";

        throw lexception(_err, _errtext);
    }


protected:

    ///Assert valid rule id
    void __assert_valid_rule( int sid ) const
    {
        if( sid == 0  ||  sid+1 >= (int)_grpary.size()  ||  -sid-1 >= (int)_stbary.size() )
        {
            _err = lexception::ERR_INVALID_RULE_ID;
            _errtext << "invalid rule id (" << sid << ")";

            throw lexception(_err, _errtext);
        }
    }

    ///Assert valid sequence-type rule id
    void __assert_valid_sequence( int sid, uchar type ) const
    {
        if( sid > 0 ) {
            _err = lexception::ERR_ENTITY_BAD_TYPE;
            _errtext << "invalid rule type (" << sid << "), a sequence-type expected";

            throw lexception(_err, _errtext);
        }
        else if( sid == 0  ||  -sid-1 >= (int)_stbary.size() )
        {
            _err = lexception::ERR_INVALID_RULE_ID;
            _errtext << "invalid rule id (" << sid << ")";

            throw lexception(_err, _errtext);
        }
        else if( type > 0 )
        {
            const sequence* seq = _stbary[-sid-1];
            if( seq->type != type ) {
                _err = lexception::ERR_ENTITY_BAD_TYPE;
                _errtext << "invalid rule type: " << seq->entity_type()
                    << " ('" << seq->name << "'), a "
                    << entity::entity_type(type) << " expected";

                throw lexception(_err, _errtext);
            }
        }
    }


////////////////////////////////////////////////////////////////////////////////
protected:

    ///Lexer entity base class
    struct entity
    {
        ///Known enity types
        enum EType {
            GROUP = 1,                  ///< character group
            ESCAPE,                     ///< escape character(s)
            KEYWORDLIST,                ///< keywords (matched by a group but having a different class when returned)
            SEQUENCE,                   ///< sequences (for string and block matching or keywords that span multiple groups)
            STRING,                     ///< sequence specialization with simple content (with escape sequences processing)
            BLOCK,                      ///< sequence specialization with recursive content
        };

        charstr name;                   ///< entity name
        uchar type;                     ///< EType values
        uchar status;                   ///< 
        ushort id;                      ///< entity id, index to containers by entity type


        entity( const token& name, uchar type, ushort id )
        : name(name), type(type), id(id)
        {
            status = 0;
        }

        ///Is a sequence, string or block
        bool is_ssb() const             { return type >= SEQUENCE; }

        bool is_sequence() const        { return type == SEQUENCE; }
        bool is_block() const           { return type == BLOCK; }
        bool is_string() const          { return type == STRING; }

        bool is_keywordlist() const     { return type == KEYWORDLIST; }

        operator token() const          { return name; }

        ///Entity name
        const char* entity_type() const { return entity_type(type); }

        static const char* entity_type( uchar type )
        {
            switch(type) {
            case 0:             return "end-of-file";
            case GROUP:         return "group";
            case ESCAPE:        return "escape";
            case KEYWORDLIST:   return "keyword";
            case SEQUENCE:      return "sequence";
            case STRING:        return "string";
            case BLOCK:         return "block";
            default:            return "unknown";
            }
        }
    };

    ///Character group descriptor
    struct group_rule : entity
    {
        short bitmap;                   ///< trailing bit map id, or -1
        bool single;                    ///< true if single characters are emitted instead of chunks

        group_rule( const token& name, ushort id, bool bsingle )
        : entity(name,entity::GROUP,id)
        {
            bitmap = -1;
            single = bsingle;
        }

        bool has_trailset() const       { return bitmap >= 0; }
    };

    ///Escape tuple for substitutions
    struct escpair
    {
        charstr code;                   ///< escape sequence (without the escape char itself)
        charstr replace;                ///< substituted string

        bool operator <  ( const token& k ) const {
            return code.len() > k.len();
        }

        void assign( const token& code, const token& replace )
        {
            this->code = code;
            this->replace = replace;
        }
    };

    ///Escape sequence translator descriptor 
    struct escape_rule : entity
    {
        char esc;                       ///< escape character
        fn_replace_esc_seq  replfn;     ///< custom replacement function

        dynarray<escpair> pairs;        ///< static replacement pairs

        ///Backward mapping from replacement to escape strings
        class back_map {
            enum { BITBLK = 8*sizeof(uint32) };

            dynarray<const escpair*>
                map;                    ///< reverted mapping of escaped symbols for synthesizer

            /// bit map for fast lookups whether replacement sequence can start with given character
            uint32 fastlookup[256/BITBLK];

            ///Sorter: longer strings place lower than shorter string they start with
            struct func {
                bool operator()(const escpair* p, const token& k) const {
                    return p->replace.cmplf(k) < 0;
                }
            };

        public:
            ///Insert escape pair into backmap
            void insert( const escpair* pair ) {
                uchar c = pair->replace.first_char();
                fastlookup[c/BITBLK] |= 1<<(c%BITBLK);
                *map.add_sortT(pair->replace, func()) = pair;
            }

            ///Find longest replacement that starts given token
            const escpair* find( const token& t ) const
            {
                if( !(fastlookup[t[0]/BITBLK] & (1<<(t[0]%BITBLK))) )
                    return 0;

                uints i = map.lower_boundT(t, func());
                uints j, n = map.size();
                for( j=i ; j<n; ++j ) {
                    if( !t.begins_with( map[j]->replace ) )
                        break;
                }
                return j>i ? map[j-1] : 0;
            }

            back_map() {
                ::memset( fastlookup, 0, sizeof(fastlookup) );
            }
        };

        mutable local<back_map>
            backmap;                    ///< backmap object, created on demand



        escape_rule( const token& name, ushort id )
        : entity(name,entity::ESCAPE,id)
        { }


        //@return true if some replacements were made and \a dst is filled,
        /// or false if no processing was required and \a dst was not filled
        bool synthesize_string( token tok, charstr& dst ) const;

    private:
        void init_backmap() const {
            if(backmap) return;

            backmap = new back_map;
            for( uint i=0; i<pairs.size(); ++i )
                backmap->insert(&pairs[i]);
        }
    };

    ///Keyword map for detection of whether token is a reserved word
    struct keywords
    {
        hash_set<charstr, token_hash>
            set;                        ///< hash_set for fast detection if the string is in the list
        int nkwd;                       ///< number of keywords


        bool has_keywords() const {
            return nkwd > 0;
        }

        bool add( const token& kwd )
        {
            bool succ;
            if( succ = (0 != set.insert_value(kwd)) )
                ++nkwd;

            return succ;
        }

        bool valid( uint hash, const token& kwd ) const
        {
            return set.find_value( hash, kwd ) != 0;
        }
    };

    ///Character sequence descriptor.
    struct sequence : entity
    {
        charstr leading;                ///< sequence of characters to be detected

        sequence( const token& name, ushort id, uchar type=entity::SEQUENCE )
        : entity(name,type,id)
        {
            DASSERT( id < xSEQ || id==WMAX );
        }
    };

    ///String and block base entity. This is the base class for compound sequences
    /// that are wrapped between one leading and possibly multiple trailing character
    /// sequences.
    struct stringorblock : sequence
    {
        ///Possible trailing sequences
        struct trail {
            charstr seq;

            void set( const token& s ) {
                seq = s;
            }
        };

        dynarray<trail> trailing;       ///< at least one trailing string/block delimiter


        void add_trailing( const token& t )
        {
            uints len = t.len();
            uints i=0;
            for( uints n=trailing.size(); i<n; ++i )
                if( len > trailing[i].seq.len() )  break;

            trailing.ins(i)->set(t);
        }

        stringorblock( const token& name, ushort id, uchar type )
        : sequence(name,id,type)
        { }
    };

    ///String descriptor. Rule that interprets all input between two character 
    /// sequences as one token, with additional escape sequence processing.
    struct string_rule : stringorblock
    {
        escape_rule* escrule;           ///< escape rule to use within the string

        string_rule( const token& name, ushort id ) : stringorblock(name,id,entity::STRING)
        { escrule = 0; }
    };

    ///Block descriptor.
    struct block_rule : stringorblock
    {
        uint64 stbenabled;              ///< bit map with sequences allowed to nest (enabled)
        uint64 stbignored;              ///< bit map with sequences skipped (ignored)

        ///Make the specified S/S/B enabled or disabled within this block.
        ///If this very same block is enabled it means that it can nest in itself.
        void enable( int id, bool en )
        {
            DASSERT( id < 64 );
            if(en)
                stbenabled |= 1ULL << id;
            else
                stbenabled &= ~(1ULL << id);
        }

        ///Make the specified S/S/B ignored or not ignored within this block.
        ///Ignored sequences are still analyzed for correctness, but are not returned.
        void ignore( int id, bool ig )
        {
            DASSERT( id < 64 );
            if(ig)
                stbignored |= 1ULL << id;
            else
                stbignored &= ~(1ULL << id);
        }

        bool enabled( int seq ) const   { return (stbenabled & (1ULL<<seq)) != 0; }
        bool ignored( int seq ) const   { return (stbignored & (1ULL<<seq)) != 0; }


        block_rule( const token& name, ushort id )
        : stringorblock(name,id,entity::BLOCK)
        {
            stbenabled = stbignored = 0;
        }
    };

    ///Root block
    struct root_block : block_rule
    {
        root_block()
        : block_rule(token::empty(),WMAX)
        { stbenabled = UMAX64; }
    };


    const entity& get_entity( int id ) const
    {
        if( id > 0 )
            return *_grpary[id-1];
        else if( id < 0 )
            return *_stbary[-id-1];
        else {
            static entity end("end-of-file", 0, 0);
            return end;
        }
    }


    bool enabled( const sequence& seq ) const   { return (*_stack.last())->enabled(seq.id); }
    bool ignored( const sequence& seq ) const   { return (*_stack.last())->ignored(seq.id); }

    bool enabled( int seq ) const               { return (*_stack.last())->enabled(-1-seq); }
    bool ignored( int seq ) const               { return (*_stack.last())->ignored(-1-seq); }


    ///Enable/disable all entities with the same (common) name.
    void enable_in_block( block_rule* br, token name, bool en )
    {
        strip_flags(name);

        Tentmap::range_const_iterator r = _entmap.equal_range(name);

        if( r.first == r.second )
            __throw_doesnt_exist(name);

        for( ; r.first!=r.second; ++r.first ) {
            const entity* ent = *r.first;
            if( ent->is_ssb() )
                br->enable(ent->id, en);
        }
    }

    ///Ignore/don't ignore all entities with the same name
    void ignore_in_block( block_rule* br, token name, bool ig )
    {
        strip_flags(name);

        Tentmap::range_const_iterator r = _entmap.equal_range(name);

        if( r.first == r.second )
            __throw_doesnt_exist(name);

        for( ; r.first!=r.second; ++r.first ) {
            const entity* ent = *r.first;
            if( ent->is_ssb() )
                br->ignore(ent->id, ig);
        }
    }

    ///Character flags
    enum {
        xGROUP                      = 0x000f,   ///< mask for the primary character group id
        fGROUP_KEYWORDS             = 0x0010,   ///< set if there are any keywords defined for the group with this leading character
        fGROUP_ESCAPE               = 0x0020,   ///< set if the character is an escape character
        fGROUP_SINGLE               = 0x0040,   ///< set if this is a single-character token emitting group
        fGROUP_TRAILSET             = 0x0080,   ///< set if the group the char belongs to has a specific trailing set defined

        GROUP_UNASSIGNED            = xGROUP,   ///< character group with characters that weren't assigned

        fSEQ_TRAILING               = 0x8000,   ///< set if this character can start a trailing sequence
        
        xSEQ                        = 0x7f00,   ///< mask for id of group of sequences that can start with this character (zero if none)
        rSEQ                        = 8,

        //default sizes
        BASIC_UTF8_CHARS            = 128,
        BINSTREAM_BUFFER_SIZE       = 256,
    };


    uchar get_group( uchar c ) const {
        return _abmap[c] & xGROUP;
    }

    ///Process the definition of a set, executing callbacks on each character included
    //@param s the set definition, characters and ranges
    //@param fnval parameter for the callback function
    //@param fn callback
    bool process_set( token s, uchar fnval, void (lexer::*fn)(uchar,uchar) )
    {
        uchar k, kprev=0;
        for( ; !s.is_empty(); kprev=k )
        {
            k = ++s;
            if( k == '.'  &&  s.first_char() == '.' )
            {
                k = s.nth_char(1);
                if(!k || !kprev)
                {
                    _err = lexception::ERR_ILL_FORMED_RANGE;
                    on_error_prefix(true);

                    _errtext << "ill-formed range: " << char(kprev) << ".." << char(k);

                    throw lexception(_err, _errtext);
                }

                if( kprev > k ) { uchar kt=k; k=kprev; kprev=kt; }
                for( int i=kprev; i<=(int)k; ++i )  (this->*fn)(i,fnval);
                s.shift_start(2);
            }
            else
                (this->*fn)(k,fnval);
        }
        return true;
    }

    ///Callback for process_set(), add character to leading bitmap detector of a group
    void fn_group( uchar i, uchar val )
    {
        _abmap[i] &= ~xGROUP;
        _abmap[i] |= val;
    }

    ///Callback for process_set(), add character to trailing bitmap detector of a group
    void fn_trail( uchar i, uchar val ) {
        _trail[i] |= val;
    }

    ///Try to match a set of strings at offset
    //@return number of the trailing string matched or -1
    //@note strings are expected to be sorted by size (longest first)
    int match_trail( const dynarray<stringorblock::trail>& str, uints& off )
    {
        if( str[0].seq.len()+off > _tok.len() )
        {
            uints n = fetch_page(_tok.len()-off, false);
            if( n < str.last()->seq.len() )  return false;
            off = 0;
        }

        const stringorblock::trail* p = str.ptr();
        const stringorblock::trail* pe = str.ptre();
        for( int i=0; p<pe; ++p,++i )
            if( !p->seq.is_empty() && _tok.begins_with(p->seq,off) )  return i;

        return -1;
    }

    ///Try to match the string at the offset
    bool match_leading( const token& tok, uints& off )
    {
        if( tok.len()+off > _tok.len() )
        {
            uints n = fetch_page(_tok.len()-off, false);
            if( n < tok.len() )  return false;
            off = 0;
        }

        return _tok.begins_with(tok,off);
    }

    ///Read next token as if it was a string with the leading sequence already read
    const lextoken& next_read_string( const string_rule& sr, uints& off, bool outermost )
    {
        const escape_rule* er = sr.escrule;

        while(1)
        {
            //find the end while processing the escape characters
            // note that the escape group must contain also the terminators
            off = count_notescape(off);
            if( off >= _tok.len() )
            {
                if( !_bin || (off=0, 0 == fetch_page(0, false)) )
                {
                    //verify if the trailing set contains empty string, which would mean
                    // that end of file is a valid terminator of the string
                    int tid = sr.trailing.last()->seq.is_empty()
                        ?  int(sr.trailing.size())-1  :  -1;

                    add_stb_segment( sr, tid, off, outermost );

                    if(tid>=0)
                        return _last;

                    _err = lexception::ERR_STRING_TERMINATED_EARLY;
                    on_error_prefix(false);

                    _errtext << "no closing sequence of '" << sr.name << "' found";

                    throw lexception(_err, _errtext);
                }

                continue;
            }

            uchar escc = _tok[off];

            if( escc == 0 )
            {
                DASSERT(0);
                //this is a syntax error, since the string wasn't properly terminated
                add_stb_segment( sr, -1, off, outermost );

                _err = lexception::ERR_STRING_TERMINATED_EARLY;
                on_error_prefix(false);

                _errtext << "no closing sequence of '" << sr.name << "' found";

                throw lexception(_err, _errtext);
            }

            //this can be either an escape character or the terminating character,
            // although possibly from another delimiter pair

            if( er && escc == er->esc )
            {
                //a regular escape sequence, flush preceding data
                if(outermost)
                    _last.tokbuf.add_from( _tok.ptr(), off );
                _tok.shift_start(off+1);  //past the escape char

                bool replaced = false;

                if(er->replfn)
                {
                    //a function was provided for translation, we should prefetch as much data as possible
                    fetch_page(_tok.len(), false);

                    replaced = er->replfn( _tok, _last.tokbuf );

                    if(!outermost)  //a part of block, do not actually build the buffer
                        _last.tokbuf.reset();
                }

                if(!replaced)
                {
                    uint i, n = (uint)er->pairs.size();
                    for( i=0; i<n; ++i )
                        if( follows( er->pairs[i].code ) )  break;
                    if( i >= n )
                    {
                        //_err = lexception::ERR_UNRECOGNIZED_ESCAPE_SEQ;
                        //_errtext << "unrecognized escape sequence " << er->esc
                        //    << token(_tok.ptr(), uint_min(3,_tok.len())) << "..";

                        //skip one char after the escape char
                        ++_tok;
                    }
                    else
                    {
                        //found
                        const escpair& ep = er->pairs[i];

                        if(outermost)
                            _last.tokbuf += ep.replace;
                        _tok.shift_start( ep.code.len() );
                    }
                }

                off = 0;
            }
            else
            {
                int k = match_trail( sr.trailing, off );
                if(k>=0)
                {
                    add_stb_segment( sr, k, off, outermost );
                    return _last;
                }

                //this wasn't our terminator, passing by
                ++off;
            }
        }
    }


    ///Read tokens until block terminating sequence is found. Composes single token out of
    /// the block content
    const lextoken& next_read_block( const block_rule& br, uints& off, bool outermost )
    {
        while(1)
        {
            //find the end while processing the nested sequences
            // note that the escape group must contain also the terminators
            off = count_notleading(off);
            if( off >= _tok.len() )
            {
                if( !_bin || (off=0, 0 == fetch_page(0, false)) )
                {
                    //verify if the trailing set contains empty string, which would mean
                    // that end of file is a valid terminator of the block
                    int tid = br.trailing.last()->seq.is_empty()
                        ?  int(br.trailing.size())-1  :  -1;

                    add_stb_segment( br, tid, off, outermost );

                    if(tid>=0)
                        return _last;

                    _err = lexception::ERR_BLOCK_TERMINATED_EARLY;
                    on_error_prefix(false);

                    _errtext << "no closing " << br.trailing[0].seq << " found";

                    throw lexception(_err, _errtext);
                }

                continue;
            }

            //a leading character of a sequence found
            uchar code = _tok[off];
            ushort x = _abmap[code];

            //test if it's our trailing sequence
            int k;
            if( (x & fSEQ_TRAILING) && (k = match_trail(br.trailing, off)) >= 0 )
            {
                //trailing string found
                add_stb_segment( br, k, off, outermost );
                return _last;
            }

            //if it's another leading sequence, find it
            if(x & xSEQ)
            {
                const dynarray<sequence*>& dseq = _seqary[((x&xSEQ)>>rSEQ)-1];
                uint i, n = (uint)dseq.size();

                for( i=0; i<n; ++i ) {
                    uint sid = dseq[i]->id;
                    if( (br.stbenabled & (1ULL<<sid))  &&  match_leading(dseq[i]->leading,off) )
                        break;
                }

                if(i<n)
                {
                    //valid & enabled sequence found, nest
                    sequence* sob = dseq[i];
                    off += sob->leading.len();

                    if( sob->type == entity::BLOCK )
                        next_read_block(
                            *(const block_rule*)sob,
                            off, false
                        );
                    else if( sob->type == entity::STRING )
                        next_read_string(
                            *(const string_rule*)sob,
                            off, false
                        );
                    
                    continue;
                }
            }

            ++off;
        }
    }

    ///Add string or block segment
    void add_stb_segment( const stringorblock& sb, int trailid, uints& off, bool final )
    {
        uints len = trailid<0  ?  0  :  sb.trailing[trailid].seq.len(); 

        if(!final)
            off += len;

        //on the terminating string
        if( _last.tokbuf.len() > 0 )
        {
            //if there's something in the buffer, append
            _last.tokbuf.add_from( _tok.ptr(), off );
            _tok.shift_start(off);
            off = 0;
            if(final)
                _last.tok = _last.tokbuf;
        }
        else if(final)
            _last.tok.set( _tok.ptr(), off );

        if(final) {
            _tok.shift_start(off + len);
            off = 0;
        }
    }

    ///Strip leading . or ! characters from rule name
    static void strip_flags( token& name )
    {
        while(!name.is_empty()) {
            char c = name.first_char();
            if( c!='.' && c!='!' )
                break;
            ++name;
        }
    }

    ///Get ignore and disable flags from name
    static void get_flags( token& name, bool* ig, bool* en )
    {
        bool ign = false;
        bool dis = false;

        while(!name.is_empty()) {
            char c = name.first_char();
            ign |= (c == '.');
            dis |= (c == '!');
            if( c!='.' && c!='!' )
                break;
            ++name;
        }

        *ig = ign;
        *en = !dis;
    }

    ucs4 get_code( uints& offs )
    {
        uchar k = _tok._ptr[offs];
        if( k < _abmap.size() )
        {
            ++offs;
            return k;
        }

        return get_utf8_code(offs);
    }

    uint prefetch_utf8( uints offs=0 )
    {
        uchar k = _tok._ptr[offs];
        if( k < _abmap.size() )
            return 1;

        uint nb = get_utf8_seq_expected_bytes( _tok.ptr()+offs );

        uints ab = _tok.len() - offs;
        if( nb > ab )
        {
            ab = fetch_page(ab, false);
            if( ab < nb )
                throw ersSYNTAX_ERROR "invalid utf8 character";
        }

        return nb;
    }

    uint prefetch_utf8( const token& t ) const
    {
        uchar k = t.first_char();
        if( k < _abmap.size() )
            return 1;

        uint nb = get_utf8_seq_expected_bytes( t.ptr() );

        if( t.len() < nb )
            throw ersSYNTAX_ERROR "invalid utf8 character";

        return nb;
    }

    ///Get utf8 code from input data. If the code crosses the buffer boundary, put anything
    /// preceeding it to the buffer and fetch the next buffer page
    ucs4 get_utf8_code( uints& offs )
    {
        uint nb = get_utf8_seq_expected_bytes( _tok.ptr()+offs );

        uints ab = _tok.len() - offs;
        if( nb > ab )
        {
            ab = fetch_page(ab, false);
            if( ab < nb )
                throw ersSYNTAX_ERROR "invalid utf8 character";

            offs = 0;
        }

        return read_utf8_seq( _tok.ptr(), offs );
    }

    ///Count characters until escape character or a possible trailing sequence character
    uints count_notescape( uints off )
    {
        const uchar* pc = (const uchar*)_tok.ptr();
        for( ; off<_tok._len; ++off )
        {
            const char* p = (const char*)pc+off;
            if( (_abmap[*p] & (fGROUP_ESCAPE|fSEQ_TRAILING)) != 0 )  break;
        }
        return off;
    }

    ///Count characters that are not leading characters of a sequence
    uints count_notleading( uints off )
    {
        const uchar* pc = (const uchar*)_tok.ptr();
        for( ; off<_tok._len; ++off )
        {
            const char* p = (const char*)pc+off;
            if( (_abmap[*p] & (fSEQ_TRAILING|xSEQ)) != 0 )  break;
        }
        return off;
    }

    uints count_intable( const token& tok, uchar grp, uints off )
    {
        const uchar* pc = (const uchar*)tok.ptr();
        for( ; off<tok._len; ++off )
        {
            const char* p = (const char*)pc+off;
            if( (_abmap[*p] & xGROUP) != grp )
                break;

            _last.upd_hash(p);
        }
        return off;
    }

    uints count_inmask( const token& tok, uchar msk, uints off )
    {
        const uchar* pc = (const uchar*)tok.ptr();
        for( ; off<tok._len; ++off )
        {
            const char* p = (const char*)pc+off;
            if( (_trail[*p] & msk) == 0 )  break;

            _last.upd_hash(p);
        }
        return off;
    }

    uints count_intable_nohash( const token& tok, uchar grp, uints off ) const
    {
        const uchar* pc = (const uchar*)tok.ptr();
        for( ; off<tok._len; ++off )
        {
            uchar c = pc[off];
            if( (_abmap[c] & xGROUP) != grp )  break;
        }
        return off;
    }

    uints count_inmask_nohash( const token& tok, uchar msk, uints off ) const
    {
        const uchar* pc = (const uchar*)tok.ptr();
        for( ; off<tok._len; ++off )
        {
            uchar c = pc[off];
            if( (_trail[c] & msk) == 0 )  break;
        }
        return off;
    }

    ///Scan input for characters from group
    //@return token with the data, an empty token if there were none or ignored, or
    /// an empty token with _ptr==0 if there are no more data
    //@param group group characters to return
    //@param ignore true if the result would be ignored, so there's no need to fill the buffer
    //@param off number of leading characters that are already considered belonging to the group
    token scan_group( uchar group, bool ignore, uints off=0 )
    {
        off = count_intable(_tok,group,off);
        if( off >= _tok.len() )
        {
            //end of buffer
            // if there's a source binstream connected, try to suck more data from it
            if(_bin)
            {
                if( fetch_page(off, ignore) > off )
                    return scan_group( group, ignore, off );
            }

            //either there is no binstream source or it's already been drained
            // return special terminating token if we are in ignore mode
            // or there is nothing in the buffer and in input
            if( ignore || (off==0 && _last.tokbuf.len()>0) )
                return token::empty();
        }

        // if there was something in the buffer, append this to it
        token res;
        if( _last.tokbuf.len() > 0 )
        {
            _last.tokbuf.add_from( _tok.ptr(), off );
            res = _last.tokbuf;
        }
        else
            res.set( _tok.ptr(), off );

        _tok.shift_start(off);
        return res;
    }

    ///Scan input for characters set in mask
    //@return token with the data, an empty token if there were none or ignored, or
    /// an empty token with _ptr==0 if there are no more data
    //@param msk to check in \a _trail array
    //@param ignore true if the result would be ignored, so there's no need to fill the buffer
    token scan_mask( uchar msk, bool ignore, uints off=0 )
    {
        off = count_inmask(_tok,msk,off);
        if( off >= _tok.len() )
        {
            //end of buffer
            // if there's a source binstream connected, try to suck more data from it
            if(_bin)
            {
                if( fetch_page(off, ignore) > off )
                    return scan_mask( msk, ignore, off );
            }

            //either there is no binstream source or it's already been drained
            // return special terminating token if we are in ignore mode
            // or there is nothing in the buffer and in input
            if( ignore || (off==0 && _last.tokbuf.len()>0) )
                return token::empty();
        }

        // if there was something in the buffer, append this to it
        token res;
        if( _last.tokbuf.len() > 0 )
        {
            _last.tokbuf.add_from( _tok.ptr(), off );
            res = _last.tokbuf;
        }
        else
            res.set( _tok.ptr(), off );

        _tok.shift_start(off);
        return res;
    }

    ///Fetch more data from input
    //@param nkeep bytes to keep in buffer
    //@param ignore true if data before last nkeep bytes should be discarded.
    /// If false, these are copied to the token buffer
    uints fetch_page( uints nkeep, bool ignore )
    {
        //save skipped data to buffer if there is already something or if instructed to do so
        uints old = _tok.len() - nkeep;
        if( _last.tokbuf.len() > 0  ||  !ignore )
            _last.tokbuf.add_from( _tok.ptr(), old );

        if(!_bin) {
            _tok.shift_start( _tok.len() - nkeep );
            return 0;
        }

        //count _lines being discarded
        if( !_binbuf.size() ) {
            _lines_processed = _binbuf.need_new(BINSTREAM_BUFFER_SIZE);
            _lines_last = _lines_processed;
        }
        else if( _tok.ptr()+old > _lines_processed ) {
            _lines += count_newlines( _lines_processed, _tok.ptr()+old );
            _lines_processed = _binbuf.ptr();
            _lines_last = _binbuf.ptr() - int(_tok.ptr()+old - _lines_last);
        }

        if(nkeep)
            xmemcpy( _binbuf.ptr(), _tok.ptr() + old, nkeep );

        uints rl = BINSTREAM_BUFFER_SIZE - nkeep;
        uints rla = rl;
        opcd e = _bin->read_raw_full( _binbuf.ptr()+nkeep, rla );

        _tok.set( _binbuf.ptr(), rl-rla+nkeep );
        _last.tok.set(_tok.ptr(), 0);

        return rl-rla+nkeep;
    }

    void add_sequence( sequence* seq )
    {
        *_stbary.add() = seq;
        dynarray<sequence*>& dseq = set_seqgroup( seq->leading.first_char() );

        uint nc = (uint)seq->leading.len();
        uint i;
        for( i=0; i<dseq.size(); ++i )
            if( dseq[i]->leading.len() < nc )  break;

        *dseq.ins(i) = seq;
    }

    dynarray<sequence*>& set_seqgroup( uchar c )
    {
        uchar k = (_abmap[c]&xSEQ) >> rSEQ;
        if(!k) {
            _seqary.add();
            k = (uchar)_seqary.size();
            _abmap[c] |= k << rSEQ;
        }

        return _seqary[k-1];
    }

    uint add_trailing( stringorblock* sob, const token& trailing )
    {
        sob->add_trailing(trailing);

        //mark the leading character of trailing token to _abmap
        _abmap[(uchar)trailing.first_char()] |= fSEQ_TRAILING;
        
        return sob->id;
    }

    ///Set end-of-input token
    const lextoken& set_end()
    {
        _last.id = 0;
        _last.tok.set_empty();
        return _last;
    }

    ///Counts newlines, detects \r \n and \r\n
    uint count_newlines( const char* p, const char* pe )
    {
        uint newlines = 0;
        char oc = _lines_oldchar;

        for( ; p<pe; ++p )
        {
            char c = *p;
            if( c == '\r' ) {
                ++newlines;
                _lines_last = p+1;
            }
            else if( c == '\n' ) {
                if( oc != '\r' )
                    ++newlines;
                _lines_last = p+1;
            }

            oc = c;
        }

        _lines_oldchar = oc;
        return newlines;
    }

    ///Helper rule/literal mapper
    template<class T>
    struct rule_map {};

protected:

    dynarray<ushort> _abmap;            ///< group flag array, fGROUP_xxx
    dynarray<uchar> _trail;             ///< mask arrays for customized group's trailing set
    uint _ntrails;                      ///< number of trailing sets defined

    typedef dynarray<sequence*> TAsequence;
    dynarray<TAsequence> _seqary;       ///< sequence (or string or block) groups with common leading character

    dynarray<group_rule*> _grpary;      ///< character groups
    dynarray<escape_rule*> _escary;     ///< escape character replacement pairs
    dynarray<sequence*> _stbary;        ///< string or block delimiters
    keywords _kwds;

    root_block _root;                   ///< root block rule containing initial enable flags
    dynarray<block_rule*> _stack;       ///< stack with open block rules, initially contains &_root

    lextoken _last;                     ///< last token read
    int _last_string;                   ///< last string type read
    mutable int _err;                   ///< last error code, see ERR_* enums
    mutable charstr _errtext;           ///< error text

    bool _utf8;                         ///< utf8 mode

    char _lines_oldchar;                ///< last character processed
    uint _lines;                        ///< number of _lines counted until _lines_processed
    const char* _lines_last;            ///< current line start
    const char* _lines_processed;       ///< characters processed in search for newlines

    token _tok;                         ///< source string to process, can point to an external source or into the _binbuf
    binstream* _bin;                    ///< source stream
    dynarray<char> _binbuf;             ///< source stream cache buffer

    int _pushback;                      ///< true if the lexer should return the previous token again (was pushed back)

    typedef hash_multikeyset<entity*, _Select_CopyPtr<entity,token> >
        Tentmap;

    Tentmap _entmap;                    ///< entity map, maps names to groups, strings, blocks etc.
};


////////////////////////////////////////////////////////////////////////////////
//@return true if some replacements were made and \a dst is filled,
/// or false if no processing was required and \a dst was not filled
inline bool lexer::escape_rule::synthesize_string( token tok, charstr& dst ) const
{
    init_backmap();
    const char* copied = tok.ptr();
    bool altered = false;

    for( ; !tok.is_empty(); )
    {
        const char* p = tok.ptr();
        const escpair* pair = backmap->find(tok);

        if(pair) {
            dst.add_from_range( copied, p );
            tok.shift_start( pair->replace.len() );
            copied = tok.ptr();

            dst.append(esc);
            dst.append( pair->code );
            altered = true;
        }
        else
            ++tok;
    }

    if( altered  &&  tok.ptr() > copied )
        dst.add_from_range( copied, tok.ptr() );

    return altered;
}

////////////////////////////////////////////////////////////////////////////////
template<> struct lexer::rule_map<int> {
    static void desc( int grp, const lexer& lex, charstr& dst ) {
        const entity& ent = lex.get_entity(grp);
        dst << ent.entity_type() << "(" << grp << ") '" << ent.name << "'";
    }
};

template<> struct lexer::rule_map<char> {
    static void desc( char c, const lexer& lex, charstr& dst ) {
        dst << "literal character '" << c << "'";
    }
};

template<> struct lexer::rule_map<token> {
    static void desc( const token& t, const lexer& lex, charstr& dst ) {
        dst << "literal string '" << t << "'";
    }
};

template<> struct lexer::rule_map<charstr> {
    static void desc( const charstr& t, const lexer& lex, charstr& dst ) {
        dst << "literal string '" << t << "'";
    }
};

template<> struct lexer::rule_map<const char*> {
    static void desc( const char* t, const lexer& lex, charstr& dst ) {
        dst << "literal string '" << t << "'";
    }
};

COID_NAMESPACE_END

#endif //__COID_COMM_LEXER__HEADER_FILE__


