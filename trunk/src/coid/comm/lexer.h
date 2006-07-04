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
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
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
    Additionally, lexer can analyze sequences of characters enclosed in custom
    delimiters, substitute escape sequences and output the strings as one token.
    Tokenizer can be used as a lexer with parser.

    It's possible to bind the lexer to a token (a string) or to a binstream
    source, in which case the lexer also performs caching.

    The method next() is used to read subsequent tokens of input data. The last token
    read can be pushed back, to be returned again next time.

    The lexer reserves some groups for internal usage. These are the:
    - the ignore group, this is skipped and not returned while tokenizing (0)
    - string delimiting group (6)
    - escape source character (7)

    The string delimiting group is used to mark characters that should switch
    the lexer into the string-scanning mode. This group is used for seeking
    the leading characters of strings. Once in the string-scanning mode, the normal
    grouping doesn't apply - escape sequence processing is performed instead.

    The escape character group contains leading character of escape sequence, and
    trailing characters of string sequences (to detect the string terminator).
**/
class lexer
{
    struct charpair
    {
        charstr _first;
        charstr _second;

        //bool operator == ( const ucs4 k ) const         { return _first == k; }
        //bool operator <  ( const ucs4 k ) const         { return _first < k; }
    };

    struct escpair
    {
        charstr _code;
        charstr _replace;

        void (*_fnc_replace)( token& t, charstr& dest );

        //bool operator == ( const ucs4 k ) const         { return _first == k; }
        //bool operator <  ( const ucs4 k ) const         { return _first < k; }

        //operator ucs4() const                           { return _first; }
    };

    ///Character flags
    enum {
        xGROUP                      = 0x0f, ///< mask for primary character group id
        fGROUP_STRING               = 0x10, ///< the group with leading string delimiters
        fGROUP_ESCAPE               = 0x20, ///< the group with escape characters and trailing string delimiters
        fGROUP_BACKSCAPE            = 0x80, ///< the group used for synthesizing strings with correct escape sequences

        GROUP_IGNORE                = 0,    ///< character group that is ignored by default
        GROUP_SINGLE                = 1,    ///< the group for characters that are returned as single-letter tokens
        GROUP_CUSTOM                = 2,    ///< first customizable group
        GROUP_UNASSIGNED            = xGROUP,

        //entity types
        xENT                        = 0xff000000,
        ENT_GROUP                   = 0x01000000,
        ENT_STRING                  = 0x02000000,

        //default sizes
        BASIC_UTF8_CHARS            = 128,
        BINSTREAM_BUFFER_SIZE       = 256,

        //errors
        ERR_CHAR_OUT_OF_RANGE       = 1,
        ERR_ILL_FORMED_RANGE        = 2,
        ERR_ENTITY_EXISTS           = 3,
    };

    dynarray<uchar> _abmap;         ///< group flag array
    dynarray<uchar> _trail;         ///< mask arrays for customized group's trailing set

    dynarray<int8> _groups;
    uint _ntrails;

    dynarray<escpair> _escary;      ///< escape character replacement pairs
    dynarray<charpair> _strdel;     ///< string delimiters

    uchar _escchar;                 ///< escape character
    uchar _utf8group;               ///< group no. for utf8 characters, > xGROUP for only allowing utf8 ext.characters in strings
    
    int _last;                      ///< group no. of the last read token, strings have negative numbers
    int _err;                       ///< last error code, see ERR_* enums

    charstr _strbuf;                ///< buffer for preprocessed strings

    token _tok;                     ///< source string to process, can point to an external source or into the _strbuf
    binstream* _bin;                ///< source stream
    dynarray<char> _binbuf;         ///< source stream cache buffer

    token _result;                  ///< last returned token
    int _pushback;                  ///< true if the lexer should return the previous token again (was pushed back)

    ///Reverted mapping of escaped symbols for synthesizer
    hash_keyset< ucs4,const escpair*,_Select_CopyPtr<escpair,ucs4> >
        _backmap;

    ///Entity map, maps names to groups and strings
    hash_map< charstr, uint, hash<token> >
        _entmap;

protected:

    uchar get_group( ucs4 c ) const
    {
        //currently only ascii
        return (c<_abmap.size() ? _abmap[c] : _utf8group) & ~fGROUP_BACKSCAPE;
    }

public:

    lexer( bool utf8 )
    {
        _bin = 0;
        _tok.set_empty();

        _escchar = '\\';
        _singlechar = 0;
        _utf8group = 0;
        _last_mask = 0;
        _last_strdel = 0;
        _pushback = 0;
        _result.set_empty();

        if(utf8)
            _abmap.need_new( BASIC_UTF8_CHARS );
        else
            _abmap.need_new( 256 );
        for( uint i=0; i<_abmap.size(); ++i )
            _abmap[i] = GROUP_UNASSIGNED;
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
        _tok.set_empty();
        _binbuf.reset();
        _last_mask = 0;
        _last_strdel = 0;
        _pushback = 0;
        _result.set_empty();
        return 0;
    }

    bool process_set( token s, uchar fnval, void (lexer::*fn)(uchar,uchar) )
    {
        uchar kprev=0;
        for( ; !s.is_empty(); ++s, kprev=k )
        {
            uchar k = s.first_char();
            if( k == '.'  &&  s.get_char(1) == '.' )
            {
                k = s.get_char(2);
                if(!k || !kprev)  { _err=ERR_ILL_FORMED_RANGE; return -1; }
                if( k>=_abmap.size() || kprev>=_abmap.size() )  { _err=ERR_CHAR_OUT_OF_RANGE; return false; }

                if( kprev < k ) { uchar kt=k; k=kprev; kprev=kt; }
                for( int i=k; i<ke; ++i )  this->*fn(i,fnval);
                s += 2;
                k = 0;
            }
            else if( k >= _abmap.size() )  { _err=ERR_CHAR_OUT_OF_RANGE; return false; }
            else
                this->*fn(i,fnval);
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

    ///Create new group named \a name with characters from \a set
    ///@return group id or -1 on error, error id is stored in the _err variable
    ///@param name group name
    ///@param set characters to include in the group, recognizes ranges when .. is found
    ///@param trailset optional character set to match after the first character from the
    /// set was found. This can be used to allow different characters after first letter of token
    int def_group( const token& name, const token& set, const token& trailset = token::empty() )
    {
        uint g = _groups.size();
        if( !_entmap.insert_value( name, ENT_GROUP | g ) )  { _err=ERR_ENTITY_EXISTS; return -1; }

        if( !process_set( set, (uchar)g, fn_group ) )  return -1;

        if( !trailset.is_empty() )
        {
            *_groups.add() = int8(_ntrails);
            _trail.needc( _abmap.size() );
            if( !process_set( trailset, 1<<_ntrails++, fn_trail ) )  return -1;
        }
        else
            *_groups.add() = -1;

        return g;
    }

    ///Create new string sequence detector named \a name, using specified leading and trailing strings
    ///@return string id or -1 on error, error id is stored in the _err variable
    ///@param name string rule name
    ///@param leading the leading string delimiter
    ///@param trailing the trailing string delimiter
    ///@param escape name of the escape rule to use for processing of escape sequences within strings
    int def_string( const token& name, const token& leading, const token& trailing, const token& escape )
    {
    }

    ///Escape sequence processor function prototype.
    ///Used to consume input after escape character and append translated characters to dst
    ///@param src source characters to translate, the token should be advanced by the consumed amount
    ///@param dst destination buffer to append the translated characters to
    typedef void (*fn_replace_esc_seq)( token& src, charstr& dst );

    ///Define an escape rule. Escape replacement pairs are added subsequently via def_escape_pair()
    ///@return the escape rule id, or -1 on error
    ///@param name the escape rule name
    ///@param escapechar the escape character used to prefix the sequences
    ///@param fn_replace pointer to function that should perform the replacement
    int def_escape( const token& name, char escapechar, fn_replace_esc_seq fn_replace = 0 )
    {
    }

    ///Define escape string mappings
    ///@return true if successful
    ///@param escrule id of the escape rule
    ///@param from source sequence that if found directly after the escape character, will be replaced with \a to
    ///@param to the replacement string
    bool def_escape_pair( int escrule, const token& from, const token& to )
    {
    }

    void add_delimiters( ucs4 leading, ucs4 trailing )
    {
        charpair* pcp = dynarray_add_sort( _strdel, leading );
        pcp->_first = leading;
        pcp->_second = trailing;

        //add leading char to GROUP_STRING and trailing char to GROUP_ESCAPE
        if( leading < _abmap.size() )
            _abmap[leading] |= fGROUP_STRING;
        else
            _utf8group |= fGROUP_STRING;

        if( trailing < _abmap.size() )
            _abmap[trailing] |= fGROUP_ESCAPE;
        else
            _utf8group |= fGROUP_ESCAPE;

        add_to_synth_map(trailing);
    }

    void add_escape_pair( ucs4 original, const token& replacement )
    {
        escpair* pep = dynarray_add_sort( _escary, original );
        pep->_first = original;
        pep->_replace = replacement;
        pep->_fnc_replace = 0;

        add_to_synth_map(replacement);
    }

    void add_escape_pair( ucs4 original, void (*fnc_replace)(token&,charstr&) )
    {
        escpair* pep = dynarray_add_sort( _escary, original );
        pep->_first = original;
        pep->_fnc_replace = fnc_replace;
    }

    void set_escape_char( char esc )
    {
        if(_escchar)
            _abmap[_escchar] &= ~fGROUP_ESCAPE;

        _escchar = esc;
        _abmap[_escchar] |= fGROUP_ESCAPE;
    }



    uint group_mask( ucs4 c ) const
    {
        return get_mask(c);
    }

    uint next_group_mask()
    {
        ucs4 k = exists_next();
        if( k == 0 )
            return 0;

        return group_mask(k);
    }


    uint group_id( ucs4 c ) const
    {
        uchar msk = get_mask(c);
        for( uint n=0; msk; msk>>=1,++n )
        {
            if( msk & 1 )
                return n;
        }

        return UMAX;
    }

    uint next_group_id()
    {
        ucs4 k = exists_next();
        if( k == 0 )
            return UMAX;

        return group_id(k);
    }

    ///return next token, reading it to the \a dst
    charstr& next( charstr& dst )
    {
        token t = next();
        if( !_strbuf.is_empty() )
            dst.takeover(_strbuf);
        else
            dst = t;
        return dst;
    }

    ///return next token, appending it to the \a dst
    charstr& next_append( charstr& dst )
    {
        token t = next();
        if( !_strbuf.is_empty() && dst.is_empty() )
            dst.takeover(_strbuf);
        else
            dst.append(t);
        return dst;
    }

    ///return next token
    /**
        Returns token of characters belonging to the same group.
        Input token is appropriately truncated from the left side.
        On error no truncation occurs, and an empty token is returned.
    **/
    token next()
    {
        if( _pushback )
        {
            _pushback = 0;
            return _result;
        }

        _strbuf.reset();

        //skip characters from the ignored group
        _result = scan_groups( fGROUP_IGNORE, true );
        if( _result.ptr() == 0 ) {
            _last_mask = 0;
            return _result;       //no more input data
        }

        uchar code = _tok[0];

        //get mask for the leading character
        uchar x = get_mask(code);

        if( x & fGROUP_STRING )
        {
            //this may be a leading string delimiter, if we find it in the register
            //delimiters can be utf8 characters
            uints off = 0;
            ucs4 k = get_code(off);
            ints ep = dynarray_contains_sorted( _strdel, k );

            if( ep >= 0 )
            {
                _last_mask = fGROUP_STRING;
                _last_strdel = k;

                _tok += off;

                //this is a leading string delimiter, [ep] points to the delimiter pair
                return next_read_string( _strdel[ep]._second, true );
            }
        }

        //clear possible fake string-delimiter marking
        x &= ~(fGROUP_STRING | fGROUP_ESCAPE);

        _last_mask = x;

        //normal token, get all characters belonging to the same group, unless this is
        // a single-char group
        if( _singlechar & x )
        {
            //force fetch utf8 data
            _result = _tok.cut_left_n( prefetch() );
            return _result;
        }

        _result = scan_groups( x, false );
        return _result;
    }

    ///Read next token as if it was a string with leading delimiter already read, with specified
    /// trailing delimiter
    ///@note A call to was_string() would return true, but the last_string_delimiter() method
    /// would return 0 after this method has been used
    token next_as_string( ucs4 upto )
    {
        uchar omsk;

        if( upto < _abmap.size() )
            omsk = _abmap[upto],  _abmap[upto] |= fGROUP_ESCAPE;
        else
            omsk = _utf8group,  _utf8group |= fGROUP_ESCAPE;

        token t = next_read_string( upto, false );

        if( upto < _abmap.size() )
            _abmap[upto] = omsk;
        else
            _utf8group = omsk;

        return t;
    }

    ///test if the next token exists
    ucs4 exists_next()
    {
        //skip characters from the ignored group
        //uints off = tok.count_intable( _abmap, fGROUP_IGNORE, false );
        _strbuf.reset();

        //skip characters from the ignored group
        token t = scan_groups( fGROUP_IGNORE, true );
        if( t.ptr() == 0 )
            return 0;       //no more input data

        uints offs=0;
        return get_utf8_code(offs);
    }

    ///test if the next token exists
    bool empty_buffer() const
    {
        //skip characters from the ignored group
        uints off = is_utf8()
            ? _tok.count_intable_utf8( _abmap.ptr(), fGROUP_IGNORE, (fGROUP_IGNORE & _utf8group)!=0 )
            : _tok.count_intable( _abmap.ptr(), fGROUP_IGNORE );
        return off >= _tok.len();
    }


    ///Push the last token back to get it next time
    void push_back()
    {
        _pushback = 1;
    }

    token get_pushback_data()
    {
        if(_pushback)
            return _result;
        return token::empty();
    }

    ///@return true if the last token was string
    bool was_string() const             { return _last_mask == fGROUP_STRING; }

    ///@return last string delimiter or 0 if it wasn't a string
    ucs4 last_string_delimiter() const  { return _last_mask == fGROUP_STRING  ?  _last_strdel : 0; }

    ///@return group mask of the last token
    uint last_mask() const              { return _last_mask; }

    ///Strip leading and trailing characters belonging to the given group
    token& strip_group( token& tok, uint grp )
    {
        uint gmsk = 1<<grp;
        for(;;)
        {
            if( (gmsk & get_mask( tok.first_char() )) == 0 )
                break;
            ++tok;
        }
        for(;;)
        {
            if( (gmsk & get_mask( tok.last_char() )) == 0 )
                break;
            tok--;
        }
        return tok;
    }


    ///@return true if some replacements were made and \a dst is filled,
    /// or false if no processing was required and \a dst was not filled
    bool synthesize_string( const token& tok, charstr& dst )
    {
        if( _backmap.size() == 0 )
            reinitialize_synth_map();

        const char* copied = tok.ptr();
        const char* p = tok.ptr();
        const char* pe = tok.ptre();

        if( !is_utf8() )
        {
            //no utf8 mode
            for( ; p<pe; ++p )
            {
                uchar c = *p;
                if( _abmap[c] & fGROUP_BACKSCAPE )
                {
                    dst.add_from_range( copied, p );
                    copied = p+1;

                    append_escape_replacement( dst, c );
                }
            }
        }
        else
        {
            //utf8 mode
            for( ; p<pe; )
            {
                uchar c = *p;
                if( ( (uchar)c < _abmap.size()  && (_abmap[c] & fGROUP_BACKSCAPE) ) )
                {
                    dst.add_from_range( copied, p );
                    copied = p+1;

                    append_escape_replacement( dst, c );
                }
                else if( ( (uchar)c >= _abmap.size()  &&  (_utf8group & fGROUP_BACKSCAPE) ) )
                {
                    dst.add_from_range( copied, p );

                    uints off = p - tok.ptr();
                    ucs4 k = tok.get_utf8(off);

                    append_escape_replacement( dst, k );

                    p = tok.ptr() + off;
                    copied = p;
                    continue;
                }

                ++p;
            }
        }

        if( copied > tok.ptr() )
        {
            dst.add_from_range( copied, pe );
            return true;
        }

        return false;
    }

    ///@return true if some replacements were made and \a dst is filled,
    /// or false if no processing was required and \a dst was not filled
    bool synthesize_char( ucs4 k, charstr& dst )
    {
        if( _backmap.size() == 0 )
            reinitialize_synth_map();

        if( !is_utf8() )
        {
            //no utf8 mode
            if( _abmap[k] & fGROUP_BACKSCAPE ) {
                append_escape_replacement( dst, k );
                return true;
            }
        }
        else
        {
            if(
                ( (uchar)k < _abmap.size()  && (_abmap[k] & fGROUP_BACKSCAPE) )
                ||
                ( (uchar)k >= _abmap.size()  &&  (_utf8group & fGROUP_BACKSCAPE) ) )
            {
                append_escape_replacement( dst, k );
                return true;
            }
        }

        return false;
    }

protected:

    ///Read next token as if it was string, with specified terminating character
    /// @param upto terminating character (must be in fGROUP_ESCAPE group)
    /// @param eat_term if true, consume the terminating character
    token next_read_string( ucs4 upto, bool eat_term )
    {
        uints off=0;

        //eat_term=true means that this got called from the next() function
        // otherwise we should set up some values here
        if(!eat_term)
        {
            _strbuf.reset();

            _last_mask = fGROUP_STRING;
            _last_strdel = 0;
        }

        //this is a leading string delimiter, [ep] points to the delimiter pair
        while(1)
        {
            //find the end while processing the escape characters
            // note that the escape group must contain also the terminators
            off = is_utf8()
                ? _tok.count_notintable_utf8( _abmap.ptr(), fGROUP_ESCAPE, true, off )
                : _tok.count_notintable( _abmap.ptr(), fGROUP_ESCAPE, off );
            //scan_nogroups( fGROUP_ESCAPE, false, true );

            if( off >= _tok.len() )
            {
                if( 0 == fetch_page( 0, false ) )
                    throw ersSYNTAX_ERROR "end of stream before a proper string termination";
                off = 0;
                continue;
            }

            uchar escc = _tok[off];

            if( escc == 0 )
            {
                //this is a syntax error, since the string wasn't properly terminated
                throw ersSYNTAX_ERROR "end of stream before a proper string termination";
                // return an empty token without eating anything from the input
                //return token::empty();
            }

            //this can be either an escape character or the terminating character,
            // although possibly from another delimiter pair

            if( escc == _escchar )
            {
                //a regular escape sequence, flush preceding data
                _strbuf.add_from( _tok.ptr(), off );
                _tok += off+1;  //past the escape char

                //get ucs4 code of following character
                off = 0;
                ucs4 k = get_code(off);

                if( k == 0 )       //invalid
                    throw ersSYNTAX_ERROR "empty escape sequence";
                    //return token::empty();

                ints p = dynarray_contains_sorted( _escary, k );
                if( p >= 0 )
                {
                    //valid escape pair found
                    //append new stuff to the buffer
                    if( _escary[p]._fnc_replace )
                    {
                        //a function was provided for translation, we should prefetch as much data as possible
                        fetch_page( _tok.len(), false );

                        _escary[p]._fnc_replace( _tok, _escary[p]._replace );
                    }

                    _strbuf += _escary[p]._replace;
                }
                //else it wasn't recognized escape character, just continue

                _tok += off;
                off = 0;
            }
            else
            {
                uints offp = off;
                ucs4 k = get_code(off);
                if( k == upto )
                {
                    //this is our terminating character
                    if( _strbuf.len() > 0 )
                    {
                        //if there's something in the buffer, append
                        _strbuf.add_from( _tok.ptr(), offp );
                        _tok += eat_term ? off : offp;

                        _result = _strbuf;
                        return _result;
                    }

                    //we don't have to copy the token because it's accessible directly
                    // from the buffer

                    _result.set( _tok.ptr(), offp );
                    _tok += eat_term ? off : offp;
                    return _result;
                }

                //this wasn't our terminator, passing by
            }
        }
    }

    void add_to_synth_map( const token& tok )
    {
        uints off=0;
        ucs4 k = tok.get_utf8(off);
        if( off < tok.len() )
            return;       //not a single-character sequence

        add_to_synth_map(k);
    }

    void add_to_synth_map( ucs4 k )
    {
        if( k < _abmap.size() )
            _abmap[k] |= fGROUP_BACKSCAPE;
        else
            _utf8group |= fGROUP_BACKSCAPE;

        if( _backmap.size() > 0 )
            _backmap.clear();
    }

    void reinitialize_synth_map()
    {
        for( uint i=0; i<_escary.size(); ++i )
        {
            const escpair& ep = _escary[i];
            token tok = ep._replace;
            uints off=0;

            tok.get_utf8(off);
            if( off < tok.len() )
                continue;   //it's not a single char replacement

            _backmap.insert_value(&ep);
        }
    }

    void append_escape_replacement( charstr& dst, ucs4 k )
    {
        const escpair* const* pp = _backmap.find_value(k);
        if(!pp)
            dst.append_utf8(k);
        else {
            dst.append(_escchar);
            dst.append( (char)(*pp)->_first );
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

        uint nb = get_utf8_char_expected_bytes( _tok.ptr()+offs );

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
        uint nb = get_utf8_char_expected_bytes( _tok.ptr()+offs );

        uints ab = _tok.len() - offs;
        if( nb > ab )
        {
            ab = fetch_page( ab, false );
            if( ab < nb )
                throw ersSYNTAX_ERROR "invalid utf8 character";

            offs = 0;
        }

        return read_utf8_char( _tok.ptr(), offs );
    }

    ///Scan input for characters of multiple groups
    ///@return token with the data, an empty token if there were none or ignored, or
    /// an empty token with _ptr==0 if there are no more data
    token scan_groups( uint msk, bool ignore )
    {
        uints off = is_utf8()
            ? _tok.count_intable_utf8( _abmap.ptr(), msk, (msk & _utf8group)!=0 )
            : _tok.count_intable( _abmap.ptr(), msk );
        if( off >= _tok.len() )
        {
            //end of buffer
            // if there's a source binstream connected, try to suck more data from it
            if(_bin)
            {
                if( fetch_page( off, ignore ) > off )
                    return scan_groups( msk, ignore );
            }

            //either there is no binstream source or it's already been drained
            // return special terminating token if we are in the ignore mode
            // or there is nothing in the buffer and in input
            if( ignore || (off==0 && _strbuf.len()>0) )
                return token(0,0);
        }

        // if there was something in the buffer, append this to it
        token res;
        if( _strbuf.len() > 0 )
        {
            _strbuf.add_from( _tok.ptr(), off );
            res = _strbuf;
        }
        else
            res.set( _tok.ptr(), off );

        _tok += off;
        return res;
    }

    uints fetch_page( uints nkeep, bool ignore )
    {
        //save skipped data to buffer if there is already something or if instructed to do so
        if( _strbuf.len() > 0  ||  !ignore )
            _strbuf.add_from( _tok.ptr(), _tok.len()-nkeep );

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

};



COID_NAMESPACE_END

#endif //__COID_COMM_LEXER__HEADER_FILE__

