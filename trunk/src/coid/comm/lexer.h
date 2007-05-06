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
#include "hash/hashmap.h"
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
    when encountered. As a token of some group is being read, a hash value is
    computed along the scanning. If any keywords are defined, an additional
    check is performed to see whether the token read has been a keyword or not.
    The token is then returned as of type ID_KEYWORDS instead of the original
    group id.

        Lexer can also detect sequences of characters that have higher priority
    than a simple grouping. It also processes strings and blocks, that are
    enclosed in custom delimiter sequences.
        With strings it performs substitution of escape character sequences and
    outputs the strings as one processed token.
        With blocks, it allows recursive processing of nested blocks, strings or
    sequences, and outputs the whole block as a single token.

    Sequences, strings and blocks can be in enabled or disabled state. This is used
    mainly to temporarily enable certain sequences that would be otherwise grouped
    with other characters and provide undesired tokens. This is used for example to
    solve the problem when < and > characters in C++ are used both as template
    argument list delimiters and operators. Parser cares for enabling and disabling
    the particular sequences when appropriate.
    

    It's possible to bind the lexer to a token (a string) source or to a binstream
    source, in which case the lexer also performs caching.

    The method next() is used to read subsequent tokens of input data. The last token
    read can be pushed back, to be returned again next time.

    The lexer uses first group as the one containing ignored whitespace characters,
    unless you provide the next() method with different group id (or none) to ignore.

    (TODBG) Internal line and character position counting for error reporting.

    (TODO) Nestable tokenization of blocks. A mode when upon encountering an opening
    block sequence, the lexer pushes its previous context to stack and returns stating
    that block is about to be read. Subsequent calls to lexer then return tokens
    from inside the block. At last the closing sequence is read and the stack is
    popped.
        Note that blocks can declare what other block and string types are enabled
    when processing their content, so the lexer can use slightly different rules
    for processing text inside the block than outside of it.
    
    This functionality will be used by the parser.
**/
class lexer
{
    ///
    struct token_hash
    {
        uint    hash;
        typedef token type_key;

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
        void inc_char( char c )
        {
            hash = (hash ^ (uint)c) + (hash<<26) + (hash>>6);
        }

        void reset()    { hash = 0; }
    };

public:

    enum { ID_KEYWORDS = 0x8000 };

    ///Lexer token
    struct lextoken
    {
        token   tok;                        ///< value string token
        int     id;                         ///< token id (group type or string type)
        charstr tokbuf;                     ///< buffer that keeps processed string
        token_hash hash;                    ///< hash value computed for tokens (but not for strings or blocks)
        uint    line;                       ///< current line number
        const char* start;                  ///< current line start
        char ch;                            ///< last read character

        bool operator == (int i) const              { return i == id; }
        bool operator == (const token& t) const     { return tok == t; }
        bool operator == (char c) const             { return tok == c; }

        bool operator != (int i) const              { return i != id; }
        bool operator != (const token& t) const     { return tok != t; }
        bool operator != (char c) const             { return tok != c; }

        operator token() const                      { return tok; }

        void swap_to_string( charstr& buf )
        {
            if( !tokbuf.is_empty() )
                buf.swap(tokbuf);
            else
                buf = tok;
        }

        void swap_to_token_or_string( token& dst, charstr& dstr )
        {
            if( !tokbuf.is_empty() )
                dstr.swap(tokbuf);
            dst = tok;
        }

        void reset()
        {
            tokbuf.reset();
            hash.reset();
        }
    };

    lexer( bool utf8 = true )
    {
        _ntrails = 0;
        _bin = 0;
        _tok.set_null();

        _utf8 = utf8;
        _last.id = 0;
        _last.tok.set_null();
        _last_string = -1;
        _err = 0;

        _pushback = 0;

        _abmap.need_new(256);
        for( uint i=0; i<_abmap.size(); ++i )
            _abmap[i] = GROUP_UNASSIGNED;
    }

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
        _binbuf.need_new(BINSTREAM_BUFFER_SIZE);
        return 0;
    }

    ///Bind input string to read from
    opcd bind( const token& tok )
    {
        _bin = 0;
        reset();
        _tok = tok;
        return 0;
    }


    ///@return true if the lexer is set up to interpret input as utf-8 characters
    bool is_utf8() const            { return _abmap.size() == BASIC_UTF8_CHARS; }

    opcd reset()
    {
        if(_bin)
            _bin->reset_read();
        _tok.set_null();
        _binbuf.reset();
        _last.id = 0;
        _last.tok.set_null();
        _last.line = 0;
        _last.start = 0;
        _last.ch = 0;
        _last_string = -1;
        _err = 0;
        _pushback = 0;
        return 0;
    }

    ///Create new group named \a name with characters from \a set
    ///@return group id or 0 on error, error id is stored in the _err variable
    ///@param name group name
    ///@param set characters to include in the group, recognizes ranges when .. is found
    ///@param trailset optional character set to match after the first character from the
    /// set was found. This can be used to allow different characters after first letter of token
    int def_group( const token& name, const token& set, const token& trailset = token::empty() )
    {
        uint g = (uint)_grpary.size();

        if( _entmap.find_value(name) )  { _err=ERR_ENTITY_EXISTS; return 0; }

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

    ///Create new group named \a name with characters from \a set that will be returned as single
    /// character tokens
    ///@return group id or 0 on error, error id is stored in the _err variable
    ///@param name group name
    ///@param set characters to include in the group, recognizes ranges when .. is found
    int def_group_single( const token& name, const token& set )
    {
        uint g = (uint)_grpary.size();

        if( _entmap.find_value(name) )  { _err=ERR_ENTITY_EXISTS; return 0; }

        group_rule* gr = new group_rule( name, (ushort)g, true );
        _entmap.insert_value(gr);

        if( !process_set( set, (uchar)g | fGROUP_SINGLE, &lexer::fn_group ) )  return 0;

        *_grpary.add() = gr;
        return g+1;
    }

    ///Define specific keywords that should be returned as a separate token type.
    ///If the characters of specific keyword fit into one character group, they are
    /// implemented as lookups into the keyword hash table after the scanner computes
    /// the hash value for the token it processed.
    ///Otherwise (when they are heterogenous) they have to be set up as sequences.
    ///@param kwd keyword to add
    ///@return ID_KEYWORDS if the keyword consists of homogenous characters from
    /// one character group, or id of a sequence created, or 0 if already defined.
    int def_keyword( const token& kwd )
    {
        if( !homogenous(kwd) )
            return def_sequence( "keywords", kwd );

        if( _kwds.add(kwd) ) {
            _abmap[(uchar)kwd.first_char()] |= fGROUP_KEYWORDS;
            return ID_KEYWORDS;
        }

        _err = ERR_KEYWORD_ALREADY_DEFINED;
        return 0;
    }

    ///Escape sequence processor function prototype.
    ///Used to consume input after escape character and append translated characters to dst
    ///@param src source characters to translate, the token should be advanced by the consumed amount
    ///@param dst destination buffer to append the translated characters to
    typedef bool (*fn_replace_esc_seq)( token& src, charstr& dst );

    ///Define an escape rule. Escape replacement pairs are added subsequently via def_escape_pair()
    ///@return the escape rule id, or -1 on error
    ///@param name the escape rule name
    ///@param escapechar the escape character used to prefix the sequences
    ///@param fn_replace pointer to function that should perform the replacement
    int def_escape( const token& name, char escapechar, fn_replace_esc_seq fn_replace = 0 )
    {
        uint g = (uint)_escary.size();

        //only one escape rule of given name
        if( _entmap.find_value(name) )  { _err=ERR_ENTITY_EXISTS; return 0; }

        escape_rule* er = new escape_rule( name, (ushort)g );
        _entmap.insert_value(er);

        er->esc = escapechar;
        er->replfn = fn_replace;

        _abmap[(uchar)escapechar] |= fGROUP_ESCAPE;

        *_escary.add() = er;
        return g+1;
    }

    ///Define escape string mappings
    ///@return true if successful
    ///@param escrule id of the escape rule
    ///@param code source sequence that if found directly after the escape character, will be replaced
    ///@param replacewith the replacement string
    bool def_escape_pair( int escrule, const token& code, const token& replacewith )
    {
        --escrule;
        if( escrule < 0  &&  escrule >= (int)_escary.size() )  return false;

        //add escape pairs, longest codes first
        escpair* ep = _escary[escrule]->pairs.add_sortT(code);
        ep->assign( code, replacewith );

        return true;
    }


    ///Create new string sequence detector named \a name, using specified leading and trailing strings.
    ///If the leading string was already defined for another string, only the trailing string is
    /// inserted to its trailing set, and escape definition is ignored.
    ///@return string id (a negative number) or 0 on error, error id is stored in the _err variable
    ///@param name string rule name
    ///@param leading the leading string delimiter
    ///@param trailing the trailing string delimiter
    ///@param escape name of the escape rule to use for processing of escape sequences within strings
    int def_string( const token& name, const token& leading, const token& trailing, const token& escape )
    {
        uint g = (uint)_stbary.size();

        entity* const* ens = _entmap.find_value(name);
        if( ens && (*ens)->type != entity::STRING )  { _err=ERR_DIFFERENT_ENTITY_EXISTS; return 0; }

        if( ens && ((string_rule*)*ens)->leading == leading ) {
            //reuse, only trailing mark is different
            g = add_trailing( ((string_rule*)*ens), trailing );
            return -1-g;
        }

        string_rule* sr = new string_rule( name, (ushort)g );
        _entmap.insert_value(sr);

        if( !escape.is_empty() )
        {
            entity* const* en = _entmap.find_value(escape);
            sr->escrule = en ? reinterpret_cast<escape_rule*>(*en) : 0;
            if( !sr->escrule )  { _err=ERR_ENTITY_DOESNT_EXIST; return 0; }
            if( sr->escrule->type != entity::ESCAPE )  { _err=ERR_ENTITY_BAD_TYPE; return 0; }
        }

        sr->leading = leading;
        add_trailing( sr, trailing );

        add_sequence(sr);
        return -1-g;
    }


    ///Create new block sequence detector named \a name, using specified leading and trailing strings
    ///@return block id (a negative number) or 0 on error, error id is stored in the _err variable
    ///@param name block rule name
    ///@param leading the leading block delimiter
    ///@param trailing the trailing block delimiter
    ///@param nested names of possibly nested blocks and strings to look and account for (separated with spaces)
    int def_block( const token& name, const token& leading, const token& trailing, token nested )
    {
        uint g = (uint)_stbary.size();

        entity* const* ens = _entmap.find_value(name);
        if( ens && (*ens)->type != entity::BLOCK )  { _err=ERR_DIFFERENT_ENTITY_EXISTS; return 0; }

        if( ens && ((block_rule*)*ens)->leading == leading ) {
            //reuse, only trailing mark is different
            g = add_trailing( ((block_rule*)*ens), trailing );
            return -1-g;
        }

        block_rule* br = new block_rule( name, (ushort)g );
        _entmap.insert_value(br);

        for(;;)
        {
            token ne = nested.cut_left(' ',1);
            if(ne.is_empty())  break;

            int rn=-1;
            if( ne.char_is_number(0) )
            {
                //special case for cross-linked blocks, id of future block rule instead of the name
                rn = ne.touint_and_shift();
                if( ne.len() )  { _err=ERR_ENTITY_DOESNT_EXIST; return 0; }
            }
            else
            {
                entity* const* en = _entmap.find_value(ne);
                stringorblock* sob = en ? reinterpret_cast<stringorblock*>(*en) : 0;
                if(!sob)  { _err=ERR_ENTITY_DOESNT_EXIST; return 0; }

                if( sob->type != entity::BLOCK  &&  sob->type != entity::STRING  &&  sob->type != entity::SEQUENCE ) {
                    _err=ERR_ENTITY_BAD_TYPE; return 0;
                }

                rn = sob->id;
            }

            br->stballowed |= 1ULL<<rn;
        }

        br->leading = leading;
        add_trailing( br, trailing );

        add_sequence(br);
        return -1-g;
    }


    ///Create new sequence detector named \a name, using specified leading and trailing strings
    ///@return block id (a negative number) or 0 on error, error id is stored in the _err variable
    ///@param name block rule name
    ///@param seq the sequence to detect, prior to basic grouping rules
    int def_sequence( const token& name, const token& seq )
    {
        uint g = (uint)_stbary.size();

        entity* const* ens = _entmap.find_value(name);
        if( ens && (*ens)->type != entity::SEQUENCE )  { _err=ERR_DIFFERENT_ENTITY_EXISTS; return 0; }

        sequence* se = new sequence( name, (ushort)g );
        _entmap.insert_value(se);

        se->leading = seq;

        add_sequence(se);
        return -1-g;
    }

    ///Enable or disable the specified sequence, string or block
    ///@note for s/s/b with same name, this applies only to the specific one
    void enable( int seqid, bool en )
    {
        uint sid = -1 - seqid;
        sequence* seq = _stbary[sid];

        seq->enable(en);
    }

    ///Make sequence, string or block ignored or not
    ///@note for s/s/b with same name, this applies only to the specific one
    void ignore( int seqid, bool ig )
    {
        uint sid = -1 - seqid;
        sequence* seq = _stbary[sid];

        seq->ignore(ig);
    }

    ///Enable/disable all entities under the same name
    void enable( const token& name, bool en )
    {
        Tentmap::range_const_iterator r = _entmap.equal_range(name);
        for( ; r.first!=r.second; ++r.first )
            (*r.first)->enable(en);
    }

    ///Ignore/don't ignore all entities under the same name
    void ignore( const token& name, bool ig )
    {
        Tentmap::range_const_iterator r = _entmap.equal_range(name);
        for( ; r.first!=r.second; ++r.first )
            (*r.first)->ignore(ig);
    }

    ///Return next token as if the block/string opening sequence was already read
    ///@note this explicit request would read the string/block content even if the
    /// string or block is disabled
    bool next_string_or_block( int sbid )
    {
        uint sid = -1 - sbid;
        sequence* seq = _stbary[sid];

        DASSERT( seq->is_block() || seq->is_string() );

        uint off=0;
        return seq->is_block()
            ? next_read_block( *(block_rule*)seq, off, true )
            : next_read_string( *(string_rule*)seq, off, true );
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

    ///Return next token from the input
    /**
        Returns token of characters identified by a lexer rule.
        Tokens of rules that are currently set to ignored are skipped.
        On error an empty token is returned and _err contains the error.

        @param ignoregrp id of the group to ignore if found at the beginning, 0 for none.
        This is used mainly to omit whitespace (group 0), or explicitly not skip it.
    **/
    const lextoken& next( uint ignoregrp=1 )
    {
        //return last token if instructed
        if( _pushback ) { _pushback = 0; return _last; }

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
            if(_bin) {
                if( fetch_page(0,0) == 0 )
                    return set_end();
            }
            else
                return set_end();
        }

        uchar code = upd_newline( _tok.ptr() );
        ushort x = _abmap[code];        //get mask for the leading character

        if(x & xSEQ)
        {
            //this could be a leading string/block delimiter, if we can find it in the register
            const dynarray<sequence*>& dseq = _seqary[(x>>rSEQ)-1];
            uint i, n = (uint)dseq.size();
            for( i=0; i<n; ++i )
                if( dseq[i]->enabled()  &&  follows(dseq[i]->leading) )  break;

            if(i<n)
            {
                sequence* seq = dseq[i];
                uint k = seq->id;
                _last.id = -1 - k;
                _last_string = k;

                _tok += seq->leading.len();

                //this is a leading string or block delimiter
                uints off=0;
                bool st;
                if( seq->type == entity::BLOCK )
                    st = next_read_block( *(const block_rule*)seq, off, true );
                else if( seq->type == entity::STRING )
                    st = next_read_string( *(const string_rule*)seq, off, true );

                if( st && seq->ignored() )    //ignored rule
                    return next(ignoregrp);

                if(!st)
                    token::empty(); //there was an error reading the string or block

                return _last;
            }
        }

        _last.id = x & xGROUP;
        ++_last.id;

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

    ///Try to match a string
    bool follows( const token& tok )
    {
        if( tok.len() > _tok.len() )
        {
            uints n = fetch_page( _tok.len(), false );
            if( n < tok.len() )  return false;
        }

        return _tok.begins_with(tok);
    }

    ///@return true if next token matches literal string
    bool match_literal( const token& val )      { return next() == val; }
    ///@return true if next token matches literal character
    bool match_literal( char c )                { return next() == c; }

    ///try to match optional literal, push back if not succeeded
    bool match_optional( const token& val )
    {
        bool succ = (next() == val);
        if(!succ) 
            push_back();
        return succ;
    }
    ///try to match optional literal, push back if not succeeded
    bool match_optional( char c )
    {
        bool succ = (next() == c);
        if(!succ) 
            push_back();
        return succ;
    }

    bool match( int grp, charstr& dst )
    {
        next();
        if( _last != grp )  return false;
        if( !_last.tokbuf.is_empty() )
            dst.takeover( _last.tokbuf );
        else
            dst = _last.tok;
        return true;
    }

    bool match_last( int grp, charstr& dst )
    {
        if( grp != _last.id )  return false;
        if( !_last.tokbuf.is_empty() )
            dst.takeover( _last.tokbuf );
        else
            dst = _last.tok;
        return true;
    }

    ///Push the last token back to be retrieved again next time
    void push_back()
    {
        _pushback = 1;
    }

    ///@return true if whole input was consumed
    bool end() const                            { return _last.id == 0; }

    ///@return last token
    const lextoken& last() const                { return _last; }


    ///Test if whole token consists of characters from one group
    ///@return 0 if not, or else the the character group (>0)
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

    ///Strip leading and trailing characters belonging to the given group
    token& strip_group( token& tok, uint grp ) const
    {
        for(;;)
        {
            if( grp == get_group( tok.first_char() ) )
                break;
            ++tok;
        }
        for(;;)
        {
            if( grp == get_group( tok.last_char() ) )
                break;
            tok--;
        }
        return tok;
    }

    ///Signal an external error (for derived classes that provide additional syntax checking)
    void set_external_error( bool set )
    {
        if(set)
            _err = ERR_EXTERNAL_ERROR;
        else
            _err = 0;
    }


////////////////////////////////////////////////////////////////////////////////
protected:

    ///Lexer entity base class
    struct entity
    {
        ///Known enity types
        enum EType {
            GROUP = 1,
            ESCAPE,
            KEYWORDLIST,
            SEQUENCE,
            STRING,
            BLOCK,
        };

        ///Entity states
        enum EState {
            ENABLED         = 1,
            IGNORED         = 2,
        };

        charstr name;                   ///< entity name
        uchar type;                     ///< EType values
        uchar status;                   ///< EState or-ed values
        ushort id;                      ///< entity id, index to containers by entity type


        entity( const token& name_, uchar type_, ushort id_ ) : name(name_), type(type_), id(id_)
        {
            status = ENABLED;
            if( name.first_char() == '.' )
                status |= IGNORED;
        }

        void enable( bool en )          { if(en) status |= ENABLED;  else status &= ~ENABLED; }
        void ignore( bool ig )          { if(ig) status |= IGNORED;  else status &= ~IGNORED; }

        bool enabled() const            { return (status&ENABLED)!=0; }
        bool ignored() const            { return (status&IGNORED)!=0; }

        bool is_block() const           { return type == BLOCK; }
        bool is_string() const          { return type == STRING; }
        bool is_sequence() const        { return type == SEQUENCE; }
        bool is_keywordlist() const     { return type == KEYWORDLIST; }

        operator token() const          { return name; }
    };

    ///Character group descriptor
    struct group_rule : entity
    {
        short bitmap;                       ///< trailing bit map id, or -1
        bool single;

        group_rule( const token& name, ushort id, bool bsingle ) : entity(name,entity::GROUP,id)
        {
            bitmap = -1;
            single = bsingle;
        }

        bool has_trailset() const       { return bitmap >= 0; }
    };

    ///Escape tuple for substitutions
    struct escpair
    {
        charstr code;
        charstr replace;
        int newlines;                   ///< number of newlines contained within code
        uint nlpast;                    ///< offset past the last newline

        //bool operator == ( const ucs4 k ) const         { return _first == k; }
        bool operator <  ( const token& k ) const   { return code.len() > k.len(); }

        //operator ucs4() const                           { return _first; }

        void assign( const token& code, const token& replace )
        {
            this->code = code;
            this->replace = replace;
            newlines = 0;
            nlpast = 0;

            char oc=0;
            const char* p = code.ptr();
            const char* pe = code.ptre();

            for( ; p<pe; ++p ) {
                char c = *p;
                if( c == '\r' )
                {   nlpast = uint(p+1-code.ptr());  ++newlines; }
                else if( c == '\n' )
                {   nlpast = uint(p+1-code.ptr());  if(oc!='\r') ++newlines; }

                oc = c;
            }
        }

        escpair() { newlines = 0;  nlpast = 0; }
    };

    ///Escape sequence translator descriptor 
    struct escape_rule : entity
    {
        char esc;                           ///< escape character
        fn_replace_esc_seq  replfn;         ///< custom replacement function

        dynarray<escpair> pairs;            ///< replacement pairs

        escape_rule( const token& name, ushort id ) : entity(name,entity::ESCAPE,id) { }
    };

    ///Keyword map for detection of whether token is a reserved word
    struct keywords
    {
        hash_set<charstr, token_hash> set;  ///< hash_set for fast detection if the string is in the list
        int nkwd;


        bool has_keywords() const           { return nkwd > 0; }

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

    ///Character sequence descriptor
    struct sequence : entity
    {
        charstr leading;                    ///< sequence of characters to be detected

        sequence( const token& name, ushort id, uchar type=entity::SEQUENCE ) : entity(name,type,id) { }
    };

    ///String and block base entity
    struct stringorblock : sequence
    {
        dynarray<charstr> trailing;         ///< at least one trailing string/block delimiter

        void add_trailing( const charstr& t )
        {
            uints len = t.len();
            uints i=0;
            for( uints n=trailing.size(); i<n; ++i )
                if( len > trailing[i].len() )  break;

            *trailing.ins(i) = t;
        }

        stringorblock( const token& name, ushort id, uchar type ) : sequence(name,id,type) { }
    };

    ///String descriptor
    struct string_rule : stringorblock
    {
        escape_rule* escrule;               ///< escape rule to use within the string

        string_rule( const token& name, ushort id ) : stringorblock(name,id,entity::STRING)
        { escrule = 0; }
    };

    ///Block descriptor
    struct block_rule : stringorblock
    {
        uint64 stballowed;                  ///< string and/or block rules allowed to nest

        block_rule( const token& name, ushort id ) : stringorblock(name,id,entity::BLOCK)
        { stballowed = 0; }
    };

    ///Character flags
    enum {
        xGROUP                      = 0x000f,   ///< mask for primary character group id
        fGROUP_KEYWORDS             = 0x0010,   ///< set if there are any keywords defined for the group and this leading character
        fGROUP_ESCAPE               = 0x0020,   ///< set if the character is either an escape character or a trailing sequence
        fGROUP_SINGLE               = 0x0040,   ///< set if this is a single-character token emitting group
        fGROUP_TRAILSET             = 0x0080,   ///< set if the group the char belongs to has a specific trailing set defined

        GROUP_UNASSIGNED            = xGROUP,   ///< character group with characters that weren't assigned

        xSEQ                        = 0xff00,   ///< mask for id of group of sequences that can start with this character (zero if none)
        rSEQ                        = 8,

        //default sizes
        BASIC_UTF8_CHARS            = 128,
        BINSTREAM_BUFFER_SIZE       = 256,

        //errors
        ERR_CHAR_OUT_OF_RANGE       = 1,
        ERR_ILL_FORMED_RANGE        = 2,
        ERR_ENTITY_EXISTS           = 3,
        ERR_ENTITY_DOESNT_EXIST     = 4,
        ERR_ENTITY_BAD_TYPE         = 5,
        ERR_STRING_TERMINATED_EARLY = 6,
        ERR_UNRECOGNIZED_ESCAPE_SEQ = 7,
        ERR_BLOCK_TERMINATED_EARLY  = 8,
        ERR_DIFFERENT_ENTITY_EXISTS = 9,
        ERR_EXTERNAL_ERROR          = 10,       ///< the error was set from outside
        ERR_KEYWORD_ALREADY_DEFINED = 11,
    };


    uchar get_group( uchar c ) const        { return _abmap[c] & xGROUP; }

    bool process_set( token s, uchar fnval, void (lexer::*fn)(uchar,uchar) )
    {
        uchar k, kprev=0;
        for( ; !s.is_empty(); kprev=k )
        {
            k = ++s;
            if( k == '.'  &&  s.first_char() == '.' )
            {
                k = s.nth_char(1);
                if(!k || !kprev)  { _err=ERR_ILL_FORMED_RANGE; return false; }

                if( kprev > k ) { uchar kt=k; k=kprev; kprev=kt; }
                for( int i=kprev; i<=(int)k; ++i )  (this->*fn)(i,fnval);
                s += 2;
            }
            else
                (this->*fn)(k,fnval);
        }
        return true;
    }

    void fn_group( uchar i, uchar val )
    {
        _abmap[i] &= ~xGROUP;
        _abmap[i] |= val;
    }

    void fn_trail( uchar i, uchar val )
    {
        _trail[i] |= val;
    }

    ///Try to match a set of strings at offset
    ///@return position of the trailing string matched or -1
    ///@note strings are expected to be sorted by size (longest first)
    int match( const dynarray<charstr>& str, uints& off )
    {
        if( str[0].len()+off > _tok.len() )
        {
            uints n = fetch_page( _tok.len()-off, false );
            if( n < str.last()->len() )  return false;
            off = 0;
        }

        const charstr* p = str.ptr();
        const charstr* pe = str.ptre();
        for( int i=0; p<pe; ++p,++i )
            if( _tok.begins_with(*p,off) )  return i;

        return -1;
    }

    ///Try to match a string at offset
    bool match( const token& tok, uints& off )
    {
        if( tok.len()+off > _tok.len() )
        {
            uints n = fetch_page( _tok.len()-off, false );
            if( n < tok.len() )  return false;
            off = 0;
        }

        return _tok.begins_with(tok,off);
    }

    ///Read next token as if it was string, with leading characters already read
    bool next_read_string( const string_rule& sr, uints& off, bool outermost )
    {
        const escape_rule* er = sr.escrule;

        while(1)
        {
            //find the end while processing the escape characters
            // note that the escape group must contain also the terminators
            off = count_notescape(off);
            if( off >= _tok.len() )
            {
                if( 0 == fetch_page( 0, false ) )
                {
                    //input terminated before the end of the string
                    add_stb_segment( sr, -1, off, outermost );
                    _err=ERR_STRING_TERMINATED_EARLY;
                    //_last.tok.set_null();
                    return false;
                }
                off = 0;
                continue;
            }

            uchar escc = _tok[off];

            if( escc == 0 )
            {
                //this is a syntax error, since the string wasn't properly terminated
                add_stb_segment( sr, -1, off, outermost );
                _err=ERR_STRING_TERMINATED_EARLY;
                //_last.tok.set_null();
                return false;
            }

            //this can be either an escape character or the terminating character,
            // although possibly from another delimiter pair

            if( er && escc == er->esc )
            {
                //a regular escape sequence, flush preceding data
                if(outermost)
                    _last.tokbuf.add_from( _tok.ptr(), off );
                _tok += off+1;  //past the escape char

                bool norepl=false;
                if( er->replfn )
                {
                    //a function was provided for translation, we should prefetch as much data as possible
                    fetch_page( _tok.len(), false );

                    norepl = er->replfn( _tok, _last.tokbuf );

                    if(!outermost)  //a part of block, do not actually build the buffer
                        _last.tokbuf.reset();
                }

                if(!norepl)
                {
                    uint i, n = (uint)er->pairs.size();
                    for( i=0; i<n; ++i )
                        if( follows( er->pairs[i].code ) )  break;
                    if( i >= n )
                    {
                        _err=ERR_UNRECOGNIZED_ESCAPE_SEQ;
                        //_last.tok.set_null();
                        return false;
                    }

                    //found, update newline info
                    const escpair& ep = er->pairs[i];
                    if( ep.newlines ) {
                        _last.line += ep.newlines;
                        _last.start = _tok.ptr() + ep.nlpast;
                        _last.ch = ep.code.last_char();
                    }

                    if(outermost)
                        _last.tokbuf += ep.replace;
                    _tok += ep.code.len();
                }

                off = 0;
            }
            else
            {
                int k = match( sr.trailing, off );
                if(k>=0)
                {
                    add_stb_segment( sr, k, off, outermost );
                    return true;
                }

                //this wasn't our terminator, passing by
                ++off;
            }
        }
    }


    ///Read next token as block
    bool next_read_block( const block_rule& br, uints& off, bool outermost )
    {
        while(1)
        {
            //find the end while processing the escape characters
            // note that the escape group must contain also the terminators
            off = count_notleading(off);
            if( off >= _tok.len() )
            {
                if( 0 == fetch_page( 0, false ) )
                {
                    add_stb_segment( br, -1, off, outermost );
                    _err=ERR_BLOCK_TERMINATED_EARLY;
                    //_last.tok.set_null();
                    return false;
                }
                off = 0;
                continue;
            }

            uchar code = _tok[off];
            ushort x = _abmap[code];

            int k;
            if( (x & fGROUP_ESCAPE)  &&  (k = match(br.trailing, off)) >= 0 )
            {
                //trailing string found
                add_stb_segment( br, k, off, outermost );
                return true;
            }

            if(x & xSEQ)
            {
                const dynarray<sequence*>& dseq = _seqary[(x>>rSEQ)-1];
                uint i, n = (uint)dseq.size();
                for( i=0; i<n; ++i ) {
                    if(!dseq[i]->enabled())  continue;

                    uint k = dseq[i]->id;
                    if( (br.stballowed & (1ULL<<k))  &&  match(dseq[i]->leading,off) )  break;
                }

                if(i<n)
                {
                    //nest
                    sequence* sob = dseq[i];
                    off += sob->leading.len();


                    bool nest;
                    if( sob->type == entity::BLOCK )
                        nest = next_read_block( *(const block_rule*)sob, off, false );
                    else if( sob->type == entity::STRING )
                        nest = next_read_string( *(const string_rule*)sob, off, false );
                    else
                        nest = true;
                    if(!nest)  return false;
                }
                else
                    ++off;
            }
            else
                ++off;
        }
    }

    ///Add string or block segment
    void add_stb_segment( const stringorblock& sb, int trailid, uints& off, bool final )
    {
        uints len = trailid<0  ?  0  :  sb.trailing[trailid].len(); 
        if(!final)
            off += len;

        //on the terminating string
        if( _last.tokbuf.len() > 0 )
        {
            //if there's something in the buffer, append
            _last.tokbuf.add_from( _tok.ptr(), off );
            _tok += off;
            off = 0;
            if(final)
                _last.tok = _last.tokbuf;
        }
        else if(final)
            _last.tok.set( _tok.ptr(), off );

        if(final) {
            _tok += off + len;
            off = 0;
        }
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
            ab = fetch_page( ab, false );
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
    /// preceeding it to the buffer and fetch next buffer page
    ucs4 get_utf8_code( uints& offs )
    {
        uint nb = get_utf8_seq_expected_bytes( _tok.ptr()+offs );

        uints ab = _tok.len() - offs;
        if( nb > ab )
        {
            ab = fetch_page( ab, false );
            if( ab < nb )
                throw ersSYNTAX_ERROR "invalid utf8 character";

            offs = 0;
        }

        return read_utf8_seq( _tok.ptr(), offs );
    }

    uchar upd_newline( const char* p )
    {
        uchar c = (uchar)*p;

        if( c == '\r' )         { _last.start = p+1;  ++_last.line; }
        else if( c == '\n' )    { _last.start = p+1;  if(_last.ch != '\r') ++_last.line; }

        _last.ch = c;
        return c;
    }

    uints count_notescape( uints off )
    {
        const uchar* pc = (const uchar*)_tok.ptr();
        for( ; off<_tok._len; ++off )
        {
            uchar c = upd_newline( (const char*)pc+off );
            if( (_abmap[c] & fGROUP_ESCAPE) != 0 )  break;
        }
        return off;
    }

    uints count_notleading( uints off )
    {
        const uchar* pc = (const uchar*)_tok.ptr();
        for( ; off<_tok._len; ++off )
        {
            uchar c = upd_newline( (const char*)pc+off );
            if( (_abmap[c] & (xSEQ|fGROUP_ESCAPE)) != 0 )  break;
        }
        return off;
    }

    uints count_intable( const token& tok, uchar grp, uints off )
    {
        const uchar* pc = (const uchar*)tok.ptr();
        for( ; off<tok._len; ++off )
        {
            uchar c = upd_newline( (const char*)pc+off );

            if( (_abmap[c] & xGROUP) != grp )
                break;

            _last.hash.inc_char(c);
        }
        return off;
    }

    uints count_inmask( const token& tok, uchar msk, uints off )
    {
        const uchar* pc = (const uchar*)tok.ptr();
        for( ; off<tok._len; ++off )
        {
            uchar c = upd_newline( (const char*)pc+off );
            if( (_trail[c] & msk) == 0 )  break;

            _last.hash.inc_char(c);
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
    ///@return token with the data, an empty token if there were none or ignored, or
    /// an empty token with _ptr==0 if there are no more data
    ///@param group group characters to return
    ///@param ignore true if the result would be ignored, so there's no need to fill the buffer
    ///@param off number of leading characters that are already considered belonging to the group
    token scan_group( uchar group, bool ignore, uints off=0 )
    {
        off = count_intable(_tok,group,off);
        if( off >= _tok.len() )
        {
            //end of buffer
            // if there's a source binstream connected, try to suck more data from it
            if(_bin)
            {
                if( fetch_page( off, ignore ) > off )
                    return scan_group( group, ignore, off );
            }

            //either there is no binstream source or it's already been drained
            // return special terminating token if we are in ignore mode
            // or there is nothing in the buffer and in input
            if( ignore || (off==0 && _last.tokbuf.len()>0) )
                return token(0,0);
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

        _tok += off;
        return res;
    }

    ///Scan input for characters set in mask
    ///@return token with the data, an empty token if there were none or ignored, or
    /// an empty token with _ptr==0 if there are no more data
    ///@param msk to check in \a _trail array
    ///@param ignore true if the result would be ignored, so there's no need to fill the buffer
    token scan_mask( uchar msk, bool ignore, uints off=0 )
    {
        off = count_inmask(_tok,msk,off);
        if( off >= _tok.len() )
        {
            //end of buffer
            // if there's a source binstream connected, try to suck more data from it
            if(_bin)
            {
                if( fetch_page( off, ignore ) > off )
                    return scan_mask( msk, ignore, off );
            }

            //either there is no binstream source or it's already been drained
            // return special terminating token if we are in ignore mode
            // or there is nothing in the buffer and in input
            if( ignore || (off==0 && _last.tokbuf.len()>0) )
                return token(0,0);
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

        _tok += off;
        return res;
    }

    ///Fetch more data from input
    ///@param nkeep bytes to keep in buffer
    ///@param ignore true if data before last nkeep bytes should be discarded.
    /// If false, these are copied to the token buffer
    uints fetch_page( uints nkeep, bool ignore )
    {
        if(!_bin)  return 0;

        //save skipped data to buffer if there is already something or if instructed to do so
        uints old = _tok.len() - nkeep;
        if( _last.tokbuf.len() > 0  ||  !ignore )
            _last.tokbuf.add_from( _tok.ptr(), old );

        if(nkeep)
            xmemcpy( _binbuf.ptr(), _tok.ptr() + old, nkeep );

        //adjust last line pointer
        _last.start -= old;

        uints rl = BINSTREAM_BUFFER_SIZE - nkeep;
        if( !_binbuf.size() )
            _binbuf.need_new(BINSTREAM_BUFFER_SIZE);

        uints rla = rl;
        opcd e = _bin->read_raw( _binbuf.ptr()+nkeep, rla );

        _tok.set( _binbuf.ptr(), rl-rla+nkeep );
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
        uchar k = _abmap[c] >> rSEQ;
        if(!k) {
            _seqary.add();
            k = (uchar)_seqary.size();
            _abmap[c] |= k << rSEQ;
        }

        return _seqary[k-1];
    }

    uint add_trailing( stringorblock* sob, const charstr& trailing )
    {
        sob->add_trailing(trailing);

        //mark the leading character of trailing token to _abmap
        _abmap[(uchar)trailing.first_char()] |= fGROUP_ESCAPE;
        
        return sob->id;
    }

    const lextoken& set_end()
    {
        _last.id = 0;
        _last.tok.set_empty();
        return _last;
    }

protected:

    dynarray<ushort> _abmap;        ///< group flag array, fGROUP_xxx
    dynarray<uchar> _trail;         ///< mask arrays for customized group's trailing set
    uint _ntrails;                  ///< number of trailing sets defined

    typedef dynarray<sequence*> TAsequence;
    dynarray<TAsequence> _seqary;   ///< sequence (or string or block) groups with common leading character

    dynarray<group_rule*> _grpary;  ///< character groups
    dynarray<escape_rule*> _escary; ///< escape character replacement pairs
    dynarray<sequence*> _stbary;    ///< string or block delimiters
    keywords _kwds;

    lextoken _last;                 ///< last token read
    int _last_string;               ///< last string type read
    int _err;                       ///< last error code, see ERR_* enums
    bool _utf8;                     ///< utf8 mode

    token _tok;                     ///< source string to process, can point to an external source or into the _binbuf
    binstream* _bin;                ///< source stream
    dynarray<char> _binbuf;         ///< source stream cache buffer

    int _pushback;                  ///< true if the lexer should return the previous token again (was pushed back)

    typedef hash_multikeyset< token, entity*, _Select_CopyPtr<entity,token> >
        Tentmap;

    Tentmap _entmap;                ///< entity map, maps names to groups, strings, blocks etc.
};



COID_NAMESPACE_END

#endif //__COID_COMM_LEXER__HEADER_FILE__

