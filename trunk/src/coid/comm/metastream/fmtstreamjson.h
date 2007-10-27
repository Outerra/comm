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
#include "../str.h"
#include "../tokenizer.h"
#include "../binstream/txtstream.h"



COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
class fmtstreamjson : public binstream
{
protected:
    binstream* _binr;
    binstream* _binw;
    //charstr _bufr;
    charstr _bufw;
    tokenizer _tokenizer;

    int _indent;

    enum {
        GROUP_IDENTIFIERS               = 1,
        GROUP_CONTROL                   = 2,
    };

    bool _sesinitr;                         ///< session has been initiated (read or write block, cleared with flush/ack)
    bool _sesinitw;


public:
    fmtstreamjson( bool utf8=false ) : _tokenizer(utf8)  {init(0,0);}
    fmtstreamjson( binstream& b, bool utf8=false ) : _tokenizer(utf8)  {init( &b, &b );}
    fmtstreamjson( binstream* br, binstream* bw, bool utf8=false ) : _tokenizer(utf8)  {init( br, bw );}
    ~fmtstreamjson()
    {
        if(_sesinitr)
            acknowledge();

        if(_sesinitw)
            flush();
    }

    void init( binstream* br, binstream* bw )
    {
        _binr = _binw = 0;
        if(bw)  bind( *bw, BIND_OUTPUT );
        if(br)  bind( *br, BIND_INPUT );

        _indent = 0;
        _sesinitr = _sesinitw = 0;

        _tokenizer.add_to_group( 0, " \t\r\n" );

        //_tokenizer.set_special_char( '\"', '\\' ); 
        _tokenizer.add_delimiters( '"', '"' );
        _tokenizer.add_delimiters( '\'', '\'' );
        _tokenizer.set_escape_char('\\');
        _tokenizer.add_escape_pair( "\"", '"' );
        _tokenizer.add_escape_pair( "'", '\'' );
        _tokenizer.add_escape_pair( "\\", '\\' );
        _tokenizer.add_escape_pair( "b", '\b' );
        _tokenizer.add_escape_pair( "f", '\f' );
        _tokenizer.add_escape_pair( "n", '\n' );
        _tokenizer.add_escape_pair( "r", '\r' );
        _tokenizer.add_escape_pair( "t", '\t' );

        //add anything that can be a part of identifier or value (strings are treated separately)
        _tokenizer.add_to_group( GROUP_IDENTIFIERS, '0', '9' );
        _tokenizer.add_to_group( GROUP_IDENTIFIERS, 'a', 'z' );
        _tokenizer.add_to_group( GROUP_IDENTIFIERS, 'A', 'Z' );
        _tokenizer.add_to_group( GROUP_IDENTIFIERS, "_.+-" );

        //characters that correspond to struct and array control tokens
        _tokenizer.add_to_group( GROUP_CONTROL, "(){}[],:", true );

        //remaining stuff to group 3, single char output
        _tokenizer.add_remaining( 3, true );
        //_tokenizer.add_to_group( 2, "()~!@#$%^&*-+=|\\?/<>`'.,;:" );

        init_tokenizer();

        set_default_separators();
    }


    virtual void init_tokenizer()
    {
        //_tokenizer.add_to_group( 2, "{}[]", true );
    }


    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return fATTR_OUTPUT_FORMATTING;
    }

    uint get_indent() const {return _indent;}
    void set_indent( uint indent ) {_indent = indent;}

    virtual opcd read_until( const substring & ss, binstream * bout, uints max_size=UMAX )
    {
        return ersNOT_IMPLEMENTED;
    }

    virtual opcd bind( binstream& bin, int io=0 )
    {
        if( io<0 )
            _binr = &bin;
        else if( io>0 )
            _binw = &bin;
        else
            _binr = _binw = &bin;
        
        if(_binr)
            return _tokenizer.bind( *_binr );
        return 0;
    }

    virtual opcd open( const token & arg )  {return _binw->open( arg );}
    virtual opcd close( bool linger=false ) {return _binw->close( linger );}
    virtual bool is_open() const            {return _binr->is_open();}
    virtual void flush()
    {
        if( _binw == NULL )
            return;

        if(!_sesinitw)
            throw ersIMPROPER_STATE;

        _sesinitw = false;
        _indent = 0;
        _bufw << "})";

        _binw->xwrite_raw( _bufw.ptr(), _bufw.len() );

        _binw->flush();
        _bufw.reset();
    }

    virtual void acknowledge( bool eat = false )
    {
        if(!eat)
        {
            token tok = _tokenizer.next();
            if( tok != '}' )
                throw ersSYNTAX_ERROR "closing } not found";

            tok = _tokenizer.next();
            if( tok == ')' )
                tok = _tokenizer.next();

            if( !tok.is_null() )
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
            _bufw << "({";
            ++_indent;
            _sesinitw = true;
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
                        _bufw += *(const float*)p;
                        break;
                    case 8:
                        _bufw += *(const double*)p;
                        break;
                    case 16:
                        _bufw += *(const long double*)p;
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

            if( tok != '{' )
                return ersSYNTAX_ERROR "opening { not found";
            
            _sesinitr = true;
        }

        token tok = _tokenizer.next();
        //if( tok.is_empty()  &&  !_tokenizer.was_string() )
        //    return ersSYNTAX_ERROR "empty token read";

        opcd e=0;
        if( t.is_array_start() )
        {
            if( t.type == type::T_CHAR  ||  t.type == type::T_BINARY )
            {
                e = (_tokenizer.was_string()  ||  _tokenizer.was_group(GROUP_IDENTIFIERS))
                    ? opcd(0) : ersSYNTAX_ERROR "expected string";
                if(!e)
                    *(uints*)p =  (t.type == type::T_BINARY) ? tok.len()/2 : tok.len();

                _tokenizer.push_back();
            }
            else if( t.type == type::T_KEY )
            {
                if( _tokenizer.was_string()  ||  _tokenizer.was_group(GROUP_IDENTIFIERS) )
                    *(uints*)p = tok.len();
                else if( tok == char('}')  ||  tok.is_null() )
                    e = ersNO_MORE;
                else
                    e = ersSYNTAX_ERROR "expected identifier";
                if(!e)
                    *(uints*)p = tok.len();
                _tokenizer.push_back();
            }
            else if( t.type == type::T_COMPOUND )
            {
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
                }
                e = (tok == char('[')) ? opcd(0) : ersSYNTAX_ERROR "expected [";
            }
            else
                e = (tok == char('[')) ? opcd(0) : ersSYNTAX_ERROR "expected [";
        }
        else if( t.is_array_end() )
        {
            if( t.type == type::T_CHAR  ||  t.type == type::T_BINARY ) {
                e = (_tokenizer.was_string()  ||  _tokenizer.was_group(GROUP_IDENTIFIERS))
                    ? opcd(0) : ersSYNTAX_ERROR "expected string";
            }
            else if( t.type == type::T_KEY ) {
                if( !_tokenizer.was_string()  &&  !_tokenizer.was_group(GROUP_IDENTIFIERS) )
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
            else {
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
                }

                e = (tok == char('{')) ? opcd(0) : ersSYNTAX_ERROR "expected {";
            }
        }
        else if( t.type == type::T_SEPARATOR )
        {
            if( trSep.is_empty() )
                _tokenizer.push_back();
            else if( tok == char('}') ) {
                _tokenizer.push_back();
                e = ersNO_MORE;
            }
            else if( tok != trSep )
                e = ersSYNTAX_ERROR "missing separator";
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
                            ucs4 d = _tokenizer.last_string_delimiter();
                            if( d == '\"' || d == '\'' )
                                *(char*)p = tok[0];
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
                        case 16: *(long double*)p = v;  break;
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
                    if( !_tokenizer.was_string() )
                        return ersSYNTAX_ERROR "expected time";

                    e = tok.todate_local( *(timet*)p );
                    if( !e && !tok.is_empty() )
                        e = ersSYNTAX_ERROR "unexpected trailing characters";
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ERRCODE: {
                    if( tok[0] != '[' )  return ersSYNTAX_ERROR;
                    ++tok;
                    token cd = tok.cut_left( ']', 1 );
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
        token tok = _tokenizer.next();

        if( tok == char(']') )
        {
            _tokenizer.push_back();
            return ersNO_MORE;
        }

        if( t.is_next_array_element() )
        {
            if( t.type != type::T_CHAR && t.type != type::T_KEY && t.type != type::T_BINARY )
            {
                if( tok != tArraySep )
                    return ersSYNTAX_ERROR "expected array separator";
            }
        }
        else
            _tokenizer.push_back();

        return 0;
    }

    virtual opcd write_array_content( binstream_container& c, uints* count )
    {
        type t = c._type;
        uints n = c._nelements;
        c.set_array_needs_separators();

        if( t.type != type::T_CHAR  &&  t.type != type::T_KEY && t.type != type::T_BINARY )
            return write_compound_array_content(c,count);

        //optimized for character and key strings
        opcd e;
        if( c.is_continuous()  &&  n != UMAX )
        {
            if( t.type == type::T_BINARY )
                e = write_binary( c.extract(n), n );
            else if( t.type == type::T_KEY )
                e = write_raw( c.extract(n), n );
            else
            {
                //_bufw.append('\"');

                token t( (const char*)c.extract(n), n );
                if( !_tokenizer.synthesize_string( t, _bufw ) )
                    _bufw += t;

                //_bufw.append('\"');

                uints len = _bufw.len();
                e = write_raw( _bufw.ptr(), len );
                _bufw.reset();
            }

            if(!e)
                *count = n;
        }
        else
            e = write_compound_array_content(c,count);

        return e;
    }

    virtual opcd read_array_content( binstream_container& c, uints n, uints* count )
    {
        type t = c._type;
        //uints n = c._nelements;
        c.set_array_needs_separators();

        if( t.type != type::T_CHAR  &&  t.type != type::T_KEY && t.type != type::T_BINARY )
            return read_compound_array_content(c,n,count);

        token tok = _tokenizer.next();

        if( !_tokenizer.was_string()  &&  !_tokenizer.was_group(GROUP_IDENTIFIERS) )
            return ersSYNTAX_ERROR;

        opcd e=0;
        if( t.type == type::T_BINARY )
            e = read_binary(tok,c,n,count);
        else
        {
            if( n != UMAX  &&  n != tok.len() )
                e = ersMISMATCHED "array size";
            else if( c.is_continuous()  &&  n != UMAX )
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

    
    opcd write_binary( const void* data, uints n )
    {
        char* buf = _bufw.get_append_buf(n*2);
        charstrconv::bin2hex( data, buf, n, 1, 0 );
        //_bufw.append_num_uint( 16, data, n, n*2, charstr::ALIGN_NUM_FILL_WITH_ZEROS );
        return 0;
    }

    opcd read_binary( token& tok, binstream_container& c, uints n, uints* count )
    {
        uints nr = n;
        if( c.is_continuous() && n!=UMAX )
            nr = charstrconv::hex2bin( tok, c.insert(n), n, ' ' );
        else {
            for(; nr>0; --nr) {
                if( charstrconv::hex2bin( tok, c.insert(1), 1, ' ' ) ) break;
            }
        }
        if( n != UMAX  &&  nr>0 )
            return ersMISMATCHED "not enough array elements";

        tok.skip_char(' ');
        if( !tok.is_empty() )
            return ersMISMATCHED "more characters after array elements read";

        *count = n - nr;

        return 0;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        return _binw->write_raw( p, len );
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        token t = _tokenizer.get_pushback_data();

        if( len != UMAX  &&  len>t.len() )
            return ersNO_MORE;

        if( t.len() < len )
            len = t.len();
        xmemcpy( p, t.ptr(), len );
        len = 0;
        return 0;
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
        tSep = sep;
        tArraySep = arraysep;
    }

    void set_default_separators()
    {
        tEol = "\n";
        tTab = "\t";
        trSep = tSep = ", ";
        tArraySep = ",";

        _tokenizer.strip_group( trSep, 0 );
    }

protected:
    token tEol;                 ///< separator between struct open/close and members
    token tTab;                 ///< indentation
    token tSep,trSep;           ///< separator between entries
    token tArraySep;            ///< separator between array elements


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
