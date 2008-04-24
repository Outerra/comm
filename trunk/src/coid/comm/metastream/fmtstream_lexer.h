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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#ifndef __COID_COMM_FMTSTREAM_COMMON__HEADER_FILE__
#define __COID_COMM_FMTSTREAM_COMMON__HEADER_FILE__

#include "fmtstream.h"
#include "../lexer.h"
#include "../txtconv.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Base class for formatting streams using lexer
class fmtstream_lexer : public fmtstream
{
public:

    /// Return formatting stream error (if any) and current line and column for error reporting purposes
    virtual opcd fmtstream_err( token* err, token* line, uint* row, uint* col )
    {
        uint n = _tokenizer.current_line( line, col );
        if(line)  *row = n;
        
        return _tokenizer.err(err) == 0  ?  opcd(0)  :  ersSYNTAX_ERROR;
    }


    virtual uint binstream_attributes( bool in0out1 ) const {
        return fATTR_OUTPUT_FORMATTING;
    }

    virtual opcd read_until( const substring& ss, binstream * bout, uints max_size=UMAX ) {
        return ersNOT_IMPLEMENTED;
    }

    virtual opcd peek_read( uint timeout )  { return _binr->peek_read(timeout); }
    virtual opcd peek_write( uint timeout ) { return _binw->peek_write(timeout); }


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

    virtual void flush()
    {
        if( _binw == NULL )
            return;

        write_buffer(true);

        _binw->flush();
    }

    virtual void acknowledge( bool eat = false )
    {
        if( !eat && !_tokenizer.end() && !_tokenizer.next().end() )
            throw ersIO_ERROR "data left in received block";
        else
            _tokenizer.reset();
    }

    virtual void reset_read()
    {
        _tokenizer.reset();
    }

    virtual void reset_write()
    {
        _binw->reset_write();
        _bufw.reset();
    }

    virtual opcd open( const token & arg )  {return _binw->open( arg );}
    virtual opcd close( bool linger=false ) {return _binw->close( linger );}
    virtual bool is_open() const            {return _binr->is_open();}


    virtual opcd write_raw( const void* p, uints& len )
    {
        return _binw->write_raw( p, len );
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        token t = _tokenizer.last();

        if( len != UMAX  &&  len>t.len() )
            return ersNO_MORE;

        if( t.len() < len )
            len = t.len();
        xmemcpy( p, t.ptr(), len );
        len = 0;
        return 0;
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
            for( ; nr>0; --nr )
                if( charstrconv::hex2bin( tok, c.insert(1), 1, ' ' ) ) break;
        }

        *count = n - nr;

        if( n != UMAX  &&  nr>0 )
            return ersMISMATCHED "not enough array elements";

        tok.skip_char(' ');
        if( !tok.is_empty() )
            return ersMISMATCHED "more characters after array elements read";

        return 0;
    }

    fmtstream_lexer(bool utf8)
        : _binr(0), _binw(0), _tokenizer(utf8)
    {}

protected:

    opcd write_buffer( bool force = false )
    {
        opcd e = 0;
        uints len = _bufw.len();

        if( len == 0 )
            return 0;

        if( force || len >= 2048 ) {
            e = write_raw( _bufw.ptr(), len );
            _bufw.reset();
        }

        return e;
    }

protected:
    binstream* _binr;                   ///< bound reading stream
    binstream* _binw;                   ///< bound writing stream

    charstr _bufw;                      ///< caching write buffer
    lexer _tokenizer;                   ///< lexer for the format
};


COID_NAMESPACE_END


#endif  // ! __COID_COMM_FMTSTREAM_COMMON__HEADER_FILE__
