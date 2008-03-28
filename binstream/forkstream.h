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
 * Robo Strycek
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


#ifndef __COID_COMM_FORKSTREAM__HEADER_FILE__
#define __COID_COMM_FORKSTREAM__HEADER_FILE__

#include "../namespace.h"
#include "binstream.h"

COID_NAMESPACE_BEGIN



////////////////////////////////////////////////////////////////////////////////
/// class similar to netstream, but does not use any header info
class forkstream : public binstream
{
	binstream* _outa;
    binstream* _outb;
    binstream* _in;

public:
    virtual ~forkstream()     { close(false); }

	forkstream() {
		_outa = _outb = _in = 0;
	}
    forkstream( binstream* bouta, binstream* boutb, binstream* bin=0 )
    {
        _outa = bouta;
        _outb = boutb;
        _in = bin;
    }

    void bind_streams( binstream* bouta, binstream* boutb, binstream* bin=0 )
    {
        _outa = bouta;
        _outb = boutb;
        _in = bin;
    }

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return in0out1 ? _outa->binstream_attributes(in0out1) : _in->binstream_attributes(in0out1);
    }

   
    virtual opcd write( const void* p, type t )
    {
        opcd e = _outa->write( p, t );
        if(!e)
            e = _outb->write( p, t );

        return e;
    }

    virtual opcd read( void* p, type t )
    {
        return _in->read( p, t );
    }

	virtual opcd write_raw( const void* p, uints& len )
	{
        uints lenb = len;
        opcd e = _outa->write_raw( p, len );
        if(!e) e = _outb->write_raw( p, lenb );

        return e;
    }
	virtual opcd read_raw( void* p, uints& len )                        { return _in->read_raw( p, len ); }


    virtual opcd write_array_separator( type t, uchar end )
    {
        opcd e = _outa->write_array_separator( t, end );
        if(!e)  e = _outb->write_array_separator( t, end );
        return e;
    }
    virtual opcd read_array_separator( type t )                         { return _in->read_array_separator(t); }


    virtual opcd write_array_content( binstream_container& c, uints* count )
    {
        opcd e = _outa->write_array_content(c,count);
        if(!e)  e = _outb->write_array_content(c,count);
        return e;
    }

    virtual opcd read_array_content( binstream_container& c, uints n, uints* count )
    {
        return _in->read_array_content(c,n,count);
    }


    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX )
    {
        return _in->read_until( ss, bout, max_size );
    }

    virtual opcd peek_read( uint timeout )      { return _in->peek_read(timeout); }
    virtual opcd peek_write( uint timeout )     { return 0; }


    virtual opcd open( const token& arg )       { return _in->open(arg); }
    virtual opcd close( bool linger )           { return _in ? _in->close(linger) : opcd(0); }
    virtual bool is_open() const                { return _in ? _in->is_open() : false; }
    virtual opcd bind( binstream& bin, int io )
    {
        if( io<0 )
            _in = &bin;
        else if( io == 1 )
            _outa = &bin;
        else if( io == 2 )
            _outb = &bin;
        else if( io == 0 )
            _in = _outa = _outb = &bin;
        else
            return ersINVALID_PARAMS;
        return 0;
    }

	virtual void flush()                        { _outa->flush();  _outb->flush(); }
	virtual void acknowledge( bool eat )        { _in->acknowledge(eat); }

    virtual void reset_read()
    {
        _in->reset_read();
    }

    virtual void reset_write()
    {
        _outa->reset_write();
        _outb->reset_write();
    }

    virtual opcd set_timeout( uint ms )         { return _in->set_timeout(ms); }

    virtual opcd seek( int seektype, int64 pos )
    {
        opcd e;
        if( seektype & fSEEK_READ )
            e = _in->seek( seektype, pos );
        else if( seektype & fSEEK_WRITE )
        {
            e = _outa->seek( seektype, pos );
            if(!e) e = _outb->seek( seektype, pos );
        }
        return e;
    }

    virtual uint64 get_written_size() const     { return _outa->get_written_size(); }
    virtual uint64 set_written_size( int64 n )  { return _outa->set_written_size(n); }

    virtual opcd overwrite_raw( uint64 pos, const void* data, uints len )
    {
        return _outa->overwrite_raw( pos, data, len );
    }
};



COID_NAMESPACE_END

#endif //__COID_COMM_FORKSTREAM__HEADER_FILE__
