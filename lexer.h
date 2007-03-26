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


#ifndef __COID_COMM_LEXER__HEADER_FILE__
#define __COID_COMM_LEXER__HEADER_FILE__

#include "namespace.h"
#include "token.h"
#include "tutf8.h"
#include "dynarray.h"
#include "hash/hashkeyset.h"
#include "hash/hashmap.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
///Class used to partition input string or stream to groups of similar characters
/**
    A lexer object must be initially configured by specifying character groups
    and sequence delimiters. The lexer then outputs tokens of characters
    from the input, with all characters in the token belonging to the same group.
    Group 0 is used as the ignore group; tokens of characters belonging there are
    not returned by default, just skipped.

        Lexer can also detect sequences of characters that have higher priority that
    simple grouping. It also processes strings and blocks, that are enclosed in
    custom delimiter sequences.
        With strings it performs substitution of escape character sequences and
    outputs the strings as one processed token.
        With blocks, it allows recursively processing of nested blocks, strings or
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

    (TODO) Reserved words are specific character sequences belonging to a specific
    lexical group, which are detected and returned as tokens of different group type.
        They should be matched only by literals in the parser.
        For this to work fast, lexer computes a hash value as it scans the characters
    for the end of the group, performing lookup afterwards into a hash table that
    indexes the keywords. The hash value is stored also with the lextoken returned
    object.

**/
class lexer
{

public:

    ///Lexer token
    struct lextoken
    {
        token   tok;                        ///< value string token
        int     id;                         ///< token id (group type or string type)
        charstr tokbuf;                     ///< buffer that keeps processed string

        bool operator == (int i) const              { return i == id; }
        bool operator == (const token& t) const     { return tok == t; }
        bool operator == (char c) const             { return tok == c; }

        bool operator != (int i) const              { return i != id; }
        bool operator != (const token& t) const     { return tok != t; }
        bool operator != (char c) const             { return tok != c; }
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


    opcd bind( binstream& bin )
    {
        _bin = 0;
        reset();

        _bin = &bin;
        _binbuf.need_new(BINSTREAM_BUFFER_SIZE);
        return 0;
    }

    opcd bind( const token& tok )
    {
        _bin = 0;
        reset();
        _tok = tok;
        return 0;
    }


    bool is_utf8() const            { return _abmap.size() == BASIC_UTF8_CHARS; }

    opcd reset()
    {
        if(_bin)
            _bin->reset();
        _tok.set_null();
        _binbuf.reset();
        _last.id = 0;
        _last.tok.set_null();
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
    ///If the characters of specific keyword fit into one character group, they are implemented
    /// as lookups into the keyword hash table after the scanner computes the hash value for the token it processed.
    ///Otherwise (when they are heterogenous) they have to be set up as sequences.
    ///@param keywordlist list of keywords (separated with spaces)
    int def_keywords( const token& keywordlist )
    {
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
        if( escrule < 0  &&  escrule >= (int)_escary.size() )  return false;

        //add escape pairs, longest codes first
        escpair* ep = _escary[escrule]->pairs.add_sortT(code);
        ep->code = code;
        ep->replace = replacewith;

        return true;
    }


    ///Create new string sequence detector named \a name, using specified leading and trailing strings
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
        sr->trailing = trailing;

        //mark the leading character of trailing token to _abmap
        _abmap[(uchar)trailing.first_char()] |= fGROUP_ESCAPE;

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
        br->trailing = trailing;

        //mark the leading character of trailing token to _abmap
        _abmap[(uchar)trailing.first_char()] |= fGROUP_ESCAPE;

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

    ///Return next token, reading it to the \a val
    int next( lextoken& val, uint ignoregrp=1 )
    {
        val.tok = next( ignoregrp );
        if( !_last.tokbuf.is_empty() )
            val.tokbuf.takeover( _last.tokbuf );
        val.id = _last.id;
        return _last.id;
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
        Returns token of characters belonging to the same group.
        Input token is appropriately truncated from the left side.
        On error no truncation occurs, and an empty token is returned.

        @param ignoregrp id of the group to ignore if found at the beginning, 0 for none
    **/
    token next( uint ignoregrp=1 )
    {
        //return last token if instructed
        if( _pushback ) { _pushback = 0; return _last.tok; }

        _last.tokbuf.reset();

        //skip characters from the ignored group
        if(ignoregrp) {
            _last.tok = scan_group( ignoregrp, true );
            if( _last.tok.ptr() == 0 ) {
                _last.id = 0;
                return _last.tok;       //no more input data
            }
        }

        uchar code = _tok[0];
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

                if( st && seq->name.first_char() == '.' )    //ignored rule
                    return next(ignoregrp);

                return _last.tok;
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
                _last.tok = _tok.cut_left_n( prefetch() );
            }
            else
                _last.tok = _tok.cut_left_n(1);
            return _last.tok;
        }

        if( x & fGROUP_TRAILSET )
            _last.tok = scan_mask( 1<<_grpary[_last.id-1]->bitmap, false, 1 );
        else
            _last.tok = scan_group( _last.id-1, false, 1 );
        return _last.tok;
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

    bool match_literal( const token& val )
    {
        lextoken tok;
        next(tok);
        return val == tok.tok;
    }

    bool match( int grp, charstr& dst )
    {
        lextoken tok;
        if( grp != next(tok) )  return false;
        if( !tok.tokbuf.is_empty() )
            dst.takeover( tok.tokbuf );
        else
            dst = tok.tok;
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

    bool end() const                            { return _last.id == 0; }

    const lextoken& last() const                { return _last; }


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


protected:

    ///Lexer entity base class
    struct entity
    {
        charstr name;
        uchar type;
        uchar status; 
        ushort id;

        enum {
            GROUP = 1,
            ESCAPE,
            SEQUENCE,
            STRING,
            BLOCK,
        };

        entity( const token& name_, uchar type_, ushort id_ ) : name(name_), type(type_), id(id_)
        {   status = 1; }

        bool enabled() const            { return status!=0; }

        operator token () const         { return name; }
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

        //bool operator == ( const ucs4 k ) const         { return _first == k; }
        bool operator <  ( const token& k ) const   { return code.len() > k.len(); }

        //operator ucs4() const                           { return _first; }
    };

    ///Escape sequence translator descriptor 
    struct escape_rule : entity
    {
        char esc;                           ///< escape character
        fn_replace_esc_seq  replfn;         ///< custom replacement function

        dynarray<escpair> pairs;            ///< replacement pairs

        escape_rule( const token& name, ushort id ) : entity(name,entity::ESCAPE,id) { }
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
        charstr leading;                    ///< leading string delimiter
        charstr trailing;                   ///< trailing string delimiter

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
        fGROUP_ESCAPE               = 0x0020,   ///< set if the character is either an escape character or a trailing sequence
        fGROUP_SINGLE               = 0x0040,   ///< set if this is a single-character token emitting group
        fGROUP_TRAILSET             = 0x0080,   ///< set if the group has specific trailing set defined

        GROUP_IGNORE                = 0,        ///< character group that is ignored by default
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
    };


    uchar get_group( uchar c ) const        { return _abmap[c] & xGROUP; }

    bool process_set( token s, uchar fnval, void (lexer::*fn)(uchar,uchar) )
    {
        uchar k, kprev=0;
        for( ; !s.is_empty(); ++s, kprev=k )
        {
            k = s.first_char();
            if( k == '.'  &&  s.nth_char(1) == '.' )
            {
                k = s.nth_char(2);
                if(!k || !kprev)  { _err=ERR_ILL_FORMED_RANGE; return false; }

                if( kprev > k ) { uchar kt=k; k=kprev; kprev=kt; }
                for( int i=kprev; i<=(int)k; ++i )  (this->*fn)(i,fnval);
                s += 2;
                k = 0;
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
                    _err=ERR_STRING_TERMINATED_EARLY;
                    _last.tok.set_null();
                    return false;
                }
                off = 0;
                continue;
            }

            uchar escc = _tok[off];

            if( escc == 0 )
            {
                //this is a syntax error, since the string wasn't properly terminated
                _err=ERR_STRING_TERMINATED_EARLY;
                _last.tok.set_null();
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
                    if(!outermost)
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
                        _last.tok.set_null();
                        return false;
                    }

                    if(outermost)
                        _last.tokbuf += er->pairs[i].replace;
                    _tok += er->pairs[i].code.len();
                }

                off = 0;
            }
            else
            {
                if( match( sr.trailing, off ) )
                {
                    add_stb_segment( sr, off, outermost );
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
                    _err=ERR_BLOCK_TERMINATED_EARLY;
                    _last.tok.set_null();
                    return false;
                }
                off = 0;
                continue;
            }

            uchar code = _tok[off];
            ushort x = _abmap[code];

            if( (x & fGROUP_ESCAPE)  &&  match(br.trailing, off) )
            {
                //trailing string found
                add_stb_segment( br, off, outermost );
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

    void add_stb_segment( const stringorblock& sb, uints& off, bool final )
    {
        if(!final)
            off += sb.trailing.len();

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
            _tok += off + sb.trailing.len();
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

    uints prefetch( uints offs=0 )
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

        return ab;
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

    uints count_notescape( uints off ) const
    {
        const uchar* pc = (const uchar*)_tok.ptr();
        for( ; off<_tok._len; ++off )
        {
            uchar c = pc[off];
            if( (_abmap[c] & fGROUP_ESCAPE) != 0 )  break;
        }
        return off;
    }

    uints count_notleading( uints off ) const
    {
        const uchar* pc = (const uchar*)_tok.ptr();
        for( ; off<_tok._len; ++off )
        {
            uchar c = pc[off];
            if( (_abmap[c] & (xSEQ|fGROUP_ESCAPE)) != 0 )  break;
        }
        return off;
    }

    uints count_intable( const token& tok, uchar grp, uints off ) const
    {
        const uchar* pc = (const uchar*)tok.ptr();
        for( ; off<tok._len; ++off )
        {
            uchar c = pc[off];
            if( (_abmap[c] & xGROUP) != grp )  break;
        }
        return off;
    }

    uints count_inmask( const token& tok, uchar msk, uints off ) const
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
        if(group)
        {
            off = count_intable(_tok,group-1,off);
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

    uints fetch_page( uints nkeep, bool ignore )
    {
        if(!_bin)  return 0;

        //save skipped data to buffer if there is already something or if instructed to do so
        if( _last.tokbuf.len() > 0  ||  !ignore )
            _last.tokbuf.add_from( _tok.ptr(), _tok.len()-nkeep );

        if(nkeep)
            xmemcpy( _binbuf.ptr(), _tok.ptr() + _tok.len() - nkeep, nkeep );

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

protected:

    dynarray<ushort> _abmap;        ///< group flag array
    dynarray<uchar> _trail;         ///< mask arrays for customized group's trailing set
    uint _ntrails;
    dynarray< dynarray<sequence*> > _seqary;    ///< sequence (or string or block) groups with common leading character

    dynarray<group_rule*> _grpary;  ///< character groups
    dynarray<escape_rule*> _escary; ///< escape character replacement pairs
    dynarray<sequence*> _stbary;    ///< string or block delimiters

    lextoken _last;
    int _last_string;               ///< last string type read
    int _err;                       ///< last error code, see ERR_* enums
    bool _utf8;                     ///< utf8 mode

    token _tok;                     ///< source string to process, can point to an external source or into the _binbuf
    binstream* _bin;                ///< source stream
    dynarray<char> _binbuf;         ///< source stream cache buffer

    int _pushback;                  ///< true if the lexer should return the previous token again (was pushed back)

    ///Reverted mapping of escaped symbols for the synthesizer
    //hash_keyset< ucs4,const escpair*,_Select_CopyPtr<escpair,ucs4> >
    //    _backmap;

    ///Entity map, maps names to groups and strings
    hash_multikeyset< token, entity*, _Select_CopyPtr<entity,token> >
        _entmap;

};



COID_NAMESPACE_END

#endif //__COID_COMM_LEXER__HEADER_FILE__

