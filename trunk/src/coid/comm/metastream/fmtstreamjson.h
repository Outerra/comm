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
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#ifndef __COID_COMM_FMTSTREAMJSON__HEADER_FILE__
#define __COID_COMM_FMTSTREAMJSON__HEADER_FILE__

#include "../namespace.h"
#include "fmtstream_lexer.h"



COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
class fmtstreamjson : public fmtstream_lexer
{
protected:
    int _indent;
    int lexid, lexstr, lexchr;

    enum {
        GROUP_IDENTIFIERS               = 1,
        GROUP_CONTROL                   = 2,
    };

    int8 _sesinitr;                        //< session has been initiated (read or write block, cleared with flush/ack)
    int8 _sesinitw;


public:
    fmtstreamjson( bool utf8=false ) : fmtstream_lexer(utf8)
    { init(0,0); }
    
    fmtstreamjson( binstream& b, bool utf8=false ) : fmtstream_lexer(utf8)
    { init( &b, &b ); }
    
    fmtstreamjson( binstream* br, binstream* bw, bool utf8=false ) : fmtstream_lexer(utf8)
    { init( br, bw ); }

    ~fmtstreamjson()
    {
        /*if(_sesinitr)
            acknowledge();

        if(_sesinitw)
            flush();*/
    }

    void init( binstream* br, binstream* bw )
    {
        _binr = _binw = 0;
        if(bw)  bind( *bw, BIND_OUTPUT );
        if(br)  bind( *br, BIND_INPUT );

        _indent = 0;
        _sesinitr = _sesinitw = 0;


        _tokenizer.def_group( "", " \t\r\n" );

        //add anything that can be a part of identifier or value (strings are treated separately)
        lexid = _tokenizer.def_group( "id", "0..9a..zA..Z_.+-" );

        int er = _tokenizer.def_escape( "esc", '\\' );
        _tokenizer.def_escape_pair( er, "\"", "\"" );
        _tokenizer.def_escape_pair( er, "'", "\'" );
        _tokenizer.def_escape_pair( er, "\\", "\\" );
        _tokenizer.def_escape_pair( er, "b", "\b" );
        _tokenizer.def_escape_pair( er, "f", "\f" );
        _tokenizer.def_escape_pair( er, "n", "\n" );
        _tokenizer.def_escape_pair( er, "r", "\r" );
        _tokenizer.def_escape_pair( er, "t", "\t" );
        _tokenizer.def_escape_pair( er, "0", token("\0",1) );

        lexstr = _tokenizer.def_string( "str", "\"", "\"", "esc" );
        lexchr = _tokenizer.def_string( "str", "\'", "\'", "esc" );

        _tokenizer.def_string( ".comment", "#", "\n", "" );
        _tokenizer.def_string( ".comment", "#", "\r\n", "" );

        _tokenizer.def_string( ".comment", "//", "\n", "" );
        _tokenizer.def_string( ".comment", "//", "\r\n", "" );

        _tokenizer.def_block( ".blkcomment", "/*", "*/", ".blkcomment" );

        //characters that correspond to struct and array control tokens
        _tokenizer.def_group_single( "ctrl", "(){}[],:" );

        set_default_separators();
    }

    virtual token fmtstream_name()          { return "fmtstreamjson"; }

    uint get_indent() const {return _indent;}
    void set_indent( uint indent ) {_indent = indent;}

    virtual void flush()
    {
        if( _binw == NULL )
            return;

        if(!_sesinitw)
            throw ersIMPROPER_STATE;

        if(_sesinitw>0)
            _bufw << '}';//"})";
        //else
        //    _bufw << ')';
        _sesinitw = 0;
        _indent = 0;

        _binw->xwrite_raw( _bufw.ptr(), _bufw.len() );

        _binw->flush();
        _bufw.reset();
    }

    virtual void acknowledge( bool eat = false )
    {
        if(!eat)
        {
            token tok = _tokenizer.next();
            if( _sesinitr>0 && tok != '}' )
                throw ersSYNTAX_ERROR "closing } not found";

            tok = _tokenizer.next();
            if( tok == ')' )
                tok = _tokenizer.next();

            if( !_tokenizer.end() )
                throw ersIO_ERROR "data left in received block";
        }
        reset_read();
    }

    virtual void reset_read()
    {
        _tokenizer.reset();
        _sesinitr = 0;
    }

    virtual void reset_write()
    {
        _bufw.reset();
        if(_binw) _binw->reset_write();

        _sesinitw = 0;
    }



    /////////////////////////////////////////////////////////////////////////////////////////////////////
    opcd write( const void* p, type t )
    {
        if(!_sesinitw)
        {
            if(t.is_array_start()) {
                //_bufw << '(';
                _sesinitw = -1;
            }
            else {
                _bufw << '{';//"({";
                _sesinitw = 1;
            }
            ++_indent;
        }

        if( t.is_array_start() )
        {
            if( t.type == type::T_KEY ) {
                write_tabs( _indent );
                _bufw.append('"');
            }
            else if( t.type == type::T_CHAR  ||  t.type == type::T_BINARY )
                _bufw.append('"');
            else {
                _bufw << char('[');
            }
        }
        else if( t.is_array_end() )
        {
            if( t.type == type::T_KEY )
                _bufw << "\" : ";
            else if( t.type == type::T_CHAR  ||  t.type == type::T_BINARY )
                _bufw << char('\"');
            else
                _bufw << char(']');
        }
        else if( t.type == type::T_STRUCTEND )
        {
            if( !t.is_nameless() ) {
                write_tabs_check( --_indent );
                _bufw << char('}');
            }
        }
        else if( t.type == type::T_STRUCTBGN )
        {
            if( !t.is_nameless() ) {
                write_tabs( _indent++ );
                _bufw << char('{');
            }
        }
        else
        {
            switch( t.type )
            {
                case type::T_INT:
                    _bufw.append_num_int( 10, p, t.get_size() );
                    break;

                case type::T_UINT:
                    _bufw.append_num_uint( 10, p, t.get_size() );
                    break;

                case type::T_KEY:
                    _bufw.append('"');
                    _bufw.add_from( (const char*)p, t.get_size() );
                    _bufw.append('"');
                break;

                case type::T_CHAR: {

                    if( !t.is_array_element() )
                    {
                        _bufw.append('\'');
                        char c = *(char*)p;
                        if( c == '\'' || c == '\\' || c == 0 )  _bufw.append('\\');
                        _bufw.append(c?c:'0');
                        _bufw.append('\'');
                    }
                    else
                    {
                        char c = *(char*)p;
                        if( c == '\"' || c == '\\' || c == 0 )  _bufw.append('\\');
                        _bufw.append(c?c:'0');
                    }

                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_FLOAT:
                    switch( t.get_size() ) {
                    case 4:
                        _bufw.append_float(*(const float*)p, 6);
                        break;
                    case 8:
                        _bufw.append_float(*(const double*)p, 10);
                        break;

                    default: throw ersSYNTAX_ERROR "unknown type"; break;
                    }
                break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BOOL:
                    if( *(bool*)p ) _bufw << "true";
                    else            _bufw << "false";
                break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_TIME: {
                    _bufw.append('"');
                    _bufw.append_date_local( *(const timet*)p );
                    _bufw.append('"');
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ANGLE: {
                    _bufw.append('"');
                    _bufw.append_angle( *(const double*)p );
                    _bufw.append('"');
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ERRCODE:
                    {
                        opcd e = (const opcd::errcode*)p;
                        token t;
                        t.set( e.error_code(), token::strnlen( e.error_code(), 5 ) );

                        _bufw << "\"[" << t;
                        if(!e)  _bufw << "]\"";
                        else {
                            _bufw << "] " << e.error_desc();
                            const char* text = e.text();
                            if(text[0])
                                _bufw << ": " << e.text() << "\"";
                            else
                                _bufw << char('"');
                        }
                    }
                break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BINARY:
                    write_binary( p, t.get_size() );
                    //_bufw.append_num_uint( 16, p, t.get_size(), t.get_size()*2, charstr::ALIGN_NUM_FILL_WITH_ZEROS );
                    break;

                case type::T_SEPARATOR:
                    _bufw << tSep;
                    break;

                case type::T_COMPOUND:
                    break;

                default: return ersSYNTAX_ERROR "unknown type"; break;
            }
        }

        uints len = _bufw.len();
        opcd e = write_raw( _bufw.ptr(), len );
        _bufw.reset();

        return e;
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    opcd read( void* p, type t )
    {
        if(!_sesinitr)
        {
            token tok = _tokenizer.next();
            if( tok == '(' )
                tok = _tokenizer.next();

            if( !t.is_array_start() ) {
                if(tok != '{')
                    return ersSYNTAX_ERROR "opening { not found";
                _sesinitr = 1;
            }
            else {
                _tokenizer.push_back();
                _sesinitr = -1;
            }
        }

        token tok = _tokenizer.next();
        //if( tok.is_empty()  &&  !_tokenizer.was_string() )
        //    return ersSYNTAX_ERROR "empty token read";

        opcd e=0;
        if( t.is_array_start() )
        {
            if( t.type == type::T_CHAR  ||  t.type == type::T_BINARY )
            {
                e = (_tokenizer.last() == lexstr  ||  _tokenizer.last() == lexid)
                    ? opcd(0) : ersSYNTAX_ERROR "expected string";
                if(!e)
                    t.set_count( (t.type == type::T_BINARY) ? tok.len()/2 : tok.len(), p );

                _tokenizer.push_back();
            }
            else if( t.type == type::T_KEY )
            {
                if(_tokenizer.last() == lexstr  ||  _tokenizer.last() == lexid)
                    t.set_count( tok.len(), p );
                else if( tok == char('}')  ||  _tokenizer.end() )
                    e = ersNO_MORE;
                else
                    e = ersSYNTAX_ERROR "expected identifier";
                if(!e)
                    t.set_count( tok.len(), p );

                _tokenizer.push_back();
            }
            else if( t.type == type::T_COMPOUND )
            {/*
                if( _tokenizer.last_string_delimiter() == '(' )
                {
                    //optional class name found
                    if(p) {
                        charstr& s = *(charstr*)p;
                        if(s.is_empty())        //return the class type if empty
                            s = tok;
                        else if( s != tok )     //otherwise compare
                            return ersSYNTAX_ERROR "class name mismatch";
                    }
                    tok = _tokenizer.next();
                }*/
                e = (tok == char('[')) ? opcd(0) : ersSYNTAX_ERROR "expected [";
            }
            else
                e = (tok == char('[')) ? opcd(0) : ersSYNTAX_ERROR "expected [";
        }
        else if( t.is_array_end() )
        {
            if( t.type == type::T_CHAR  ||  t.type == type::T_BINARY ) {
                e = (_tokenizer.last() == lexstr  ||  _tokenizer.last() == lexid)
                    ? opcd(0) : ersSYNTAX_ERROR "expected string";
            }
            else if( t.type == type::T_KEY ) {
                if( !(_tokenizer.last() == lexstr  ||  _tokenizer.last() == lexid) )
                    return ersSYNTAX_ERROR "expected identifier";
                
                tok = _tokenizer.next();
                e = (tok == char(':'))  ?  opcd(0) : ersSYNTAX_ERROR "expected :";
            }
            else
                e = (tok == char(']')) ? opcd(0) : ersSYNTAX_ERROR "expected ]";
        }
        else if( t.type == type::T_STRUCTEND )
        {
            if( t.is_nameless() )
                _tokenizer.push_back();
            else
                e = (tok == char('}')) ? opcd(0) : ersSYNTAX_ERROR "expected }";
        }
        else if( t.type == type::T_STRUCTBGN )
        {
            if( t.is_nameless() )
                _tokenizer.push_back();
            else {/*
                if( _tokenizer.last_string_delimiter() == '(' )
                {
                    //optional class name found
                    if(p) {
                        charstr& s = *(charstr*)p;
                        if(s.is_empty())        //return the class type if empty
                            s = tok;
                        else if( s != tok )     //otherwise compare
                            return ersSYNTAX_ERROR "class name mismatch";
                    }
                    tok = _tokenizer.next();
                }*/

                e = (tok == char('{')) ? opcd(0) : ersSYNTAX_ERROR "expected {";
            }
        }
        else if( t.type == type::T_SEPARATOR )
        {
            if( trSep.is_empty() )
                _tokenizer.push_back();
            else {
                bool has = tok == trSep;
                if(has)
                    tok = _tokenizer.next();

                _tokenizer.push_back();

                if( tok == char('}') )
                    e = ersNO_MORE;
                else if(!has)
                    e = ersSYNTAX_ERROR "missing separator";
            }
        }
        else if( tok.is_empty() )
            e = ersSYNTAX_ERROR "empty token read";
        else
        {
            switch( t.type )
            {
			case type::T_INT:
				{
					token::tonum<int64> conv;
					int64 v = conv.xtoint(tok);

					if( conv.failed() )
						return ersSYNTAX_ERROR " expected number";

					if( !tok.is_empty() )
						return ersSYNTAX_ERROR " unrecognized characters after number";

					if( !valid_int_range(v,t.get_size()) )
						return ersINTEGER_OVERFLOW;

					switch( t.get_size() )
					{
					case 1: *(int8*)p = (int8)v;  break;
					case 2: *(int16*)p = (int16)v;  break;
					case 4: *(int32*)p = (int32)v;  break;
					case 8: *(int64*)p = (int64)v;  break;
					}
				}
				break;

			case type::T_UINT:
				{
					token::tonum<uint64> conv;
					uint64 v = conv.xtouint(tok);

					if( conv.failed() )
						return ersSYNTAX_ERROR " expected number";

					if( !tok.is_empty() )
						return ersSYNTAX_ERROR " unrecognized characters after number";

					if( !valid_uint_range(v,t.get_size()) )
						return ersINTEGER_OVERFLOW;

					switch( t.get_size() )
					{
					case 1: *(uint8*)p = (uint8)v;  break;
					case 2: *(uint16*)p = (uint16)v;  break;
					case 4: *(uint32*)p = (uint32)v;  break;
					case 8: *(uint64*)p = (uint64)v;  break;
					}
				}
				break;
                case type::T_KEY:
                    return ersUNAVAILABLE "should be read as array";
                    break;

                case type::T_CHAR:
                    {
                        if( !t.is_array_element() )
                        {
                            if( _tokenizer.last() == lexchr )
                                *(char*)p = tok.first_char();
                            else
                                e = ersSYNTAX_ERROR "expected string";
                        }
                        else
                            //this is optimized in read_array(), non-continuous containers not supported
                            e = ersNOT_IMPLEMENTED;
                    }
                    break;

                case type::T_FLOAT:
                    {
                        double v = tok.todouble_and_shift();
                        if( !tok.is_empty() )
                            return ersSYNTAX_ERROR "unrecognized characters after number";

                        switch( t.get_size() )
                        {
                        case 4: *(float*)p = (float)v;  break;
                        case 8: *(double*)p = v;  break;
                        }
                    }
                    break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BOOL: {
                    if( tok.cmpeqi("true") )
                        *(bool*)p = true;
                    else if( tok.cmpeqi("false") )
                        *(bool*)p = false;
                    else
                        return ersSYNTAX_ERROR "unexpected char";
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_TIME: {
                    if( _tokenizer.last() != lexstr )
                        return ersSYNTAX_ERROR "expected time";

                    e = tok.todate_local( *(timet*)p );
                    if( !e && !tok.is_empty() )
                        e = ersSYNTAX_ERROR "unexpected trailing characters";
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ANGLE: {
                    if( _tokenizer.last() != lexstr )
                        return ersSYNTAX_ERROR "expected angle";

                    *(double*)p = tok.toangle();
                    if(!tok.is_empty())
                        e = ersSYNTAX_ERROR "unexpected trailing characters";
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ERRCODE: {
                    if( tok[0] != '[' )  return ersSYNTAX_ERROR;
                    ++tok;
                    token cd = tok.cut_left(']');
                    uints n = opcd::find_code( cd.ptr(), cd.len() );
                    *(ushort*)p = (ushort)n;
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BINARY: {
                    uints i = charstrconv::hex2bin( tok, p, t.get_size(), ' ' );
                    if(i>0)
                        return ersMISMATCHED "not enough array elements";
                    tok.skip_char(' ');
                    if( !tok.is_empty() )
                        return ersMISMATCHED "more characters after array elements read";
                } break;


                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_COMPOUND:
                    break;


                default:
                    return ersSYNTAX_ERROR "unknown type";
                    break;
            }
        }

        return e;
    }


    virtual opcd write_array_separator( type t, uchar end )
    {
        if( !end && t.is_next_array_element() )
        {
            if( t.type != type::T_CHAR && t.type != type::T_KEY && t.type != type::T_BINARY )
                _bufw << tArraySep << char(' ');
        }
        return 0;
    }

    virtual opcd read_array_separator( type t )
    {
        DASSERT( t.type != type::T_CHAR && t.type != type::T_KEY && t.type != type::T_BINARY );

        token tok = _tokenizer.next();
        bool has = tok == trArraySep;
        if(has)
            tok = _tokenizer.next();

        _tokenizer.push_back();

        if( tok == char(']') )
            return ersNO_MORE;

        if( t.is_next_array_element() && !trArraySep.is_empty() )
        {
            if(!has)
                return ersSYNTAX_ERROR "expected array separator";
        }

        return 0;
    }

    virtual opcd write_array_content( binstream_container_base& c, uints* count )
    {
        type t = c._type;
        uints n = c.count();
        c.set_array_needs_separators();

        if( t.type != type::T_CHAR  &&  t.type != type::T_KEY && t.type != type::T_BINARY )
            return write_compound_array_content(c,count);

        //optimized for character and key strings
        opcd e;
        if( c.is_continuous()  &&  n != UMAXS )
        {
            if( t.type == type::T_BINARY )
                e = write_binary( c.extract(n), n );
            else if( t.type == type::T_KEY )
                e = write_raw( c.extract(n), n );
            else
            {
                token t( (const char*)c.extract(n), n );
                if( !_tokenizer.synthesize_string( lexstr, t, _bufw ) )
                    _bufw += t;

                uints len = _bufw.len();
                e = write_raw( _bufw.ptr(), len );
                _bufw.reset();
            }

            if(!e)  *count = n;
        }
        else
            e = write_compound_array_content(c,count);

        return e;
    }

    virtual opcd read_array_content( binstream_container_base& c, uints n, uints* count )
    {
        type t = c._type;
        //uints n = c._nelements;
        c.set_array_needs_separators();

        if( t.type != type::T_CHAR  &&  t.type != type::T_KEY && t.type != type::T_BINARY )
            return read_compound_array_content(c,n,count);

        token tok = _tokenizer.next();

        if( !(_tokenizer.last() == lexstr  ||  _tokenizer.last() == lexid) )
            return ersSYNTAX_ERROR;

        opcd e=0;
        if( t.type == type::T_BINARY )
            e = read_binary(tok,c,n,count);
        else
        {
            if( n != UMAXS  &&  n != tok.len() )
                e = ersMISMATCHED "array size";
            else if( c.is_continuous()  &&  n != UMAXS )
                xmemcpy( c.insert(n), tok.ptr(), tok.len() );
            else
            {
                const char* p = tok.ptr();
                uints n = tok.len();
                for(; n>0; --n,++p )  *(char*)c.insert(1) = *p;
            }

            *count = tok.len();
        }

        //push back so the trailing delimiter would match
        _tokenizer.push_back();
        return e;
    }


    ///Set separators to use
    /// @param eol separator between struct open/close and members
    /// @param tab indentation
    /// @param sep separator between standalone entries (multiple types pushed sequentially)
    /// @param arraysep separator between array elements
    void set_separators( token eol, token tab, token sep=", ", token arraysep = "," )
    {
        tEol = eol;
        tTab = tab;
        trSep = tSep = sep;
        trArraySep = tArraySep = arraysep;

        _tokenizer.strip_group( trSep, 1 );
        _tokenizer.strip_group( trArraySep, 1 );
    }

    void set_default_separators()
    {
        tEol = "\n";
        tTab = "\t";
        trSep = tSep = ", ";
        trArraySep = tArraySep = ",";

        _tokenizer.strip_group( trSep, 1 );
        _tokenizer.strip_group( trArraySep, 1 );
    }

protected:
    token tEol;                 //< separator between struct open/close and members
    token tTab;                 //< indentation
    token tSep,trSep;           //< separator between entries
    token tArraySep,trArraySep; //< separator between array elements


    void write_tabs( int indent )
    {
        _bufw << tEol;
        for( int i=0; i<indent; i++ )
            _bufw << tTab;
    }

    void write_tabs_check( int indent )
    {
        _bufw << tEol;
        if( indent < 0 )
            throw ersSYNTAX_ERROR "unexpected end of struct";
        for( int i=0; i<indent; i++ )
            _bufw << tTab;
    }
};


COID_NAMESPACE_END


#endif  // ! __COID_COMM_FMTSTREAMJSON__HEADER_FILE__
