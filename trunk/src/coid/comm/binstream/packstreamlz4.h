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
 * The Initial Developer of the Original Code is Brano Kemen
  * Portions created by the Initial Developer are Copyright (C) 2005
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


#ifndef __COID_COMM_PACKSTREAMLZ4__HEADER_FILE__
#define __COID_COMM_PACKSTREAMLZ4__HEADER_FILE__

#include "../namespace.h"

#include "packstream.h"

extern "C" {
#include "../coder/lz4/lz4.h"
//#include "../coder/lz4/lz4hc.h"
}

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///
class packstreamlz4 : public packstream
{
    static const int BLOCKSIZE = 1024*1024;

public:
    virtual ~packstreamlz4()
    {
        if(_writebuf.size() > _wlead)
            flush();
        LZ4_free(_LZ4_Data);
    }

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return 0;
    }

    virtual opcd peek_read( uint timeout ) override {
        return ersNO_MORE;
    }

    virtual opcd peek_write( uint timeout ) override {
        return 0;
    }

    virtual void reset_read() {}
    virtual void reset_write()
    {
        _out->reset_write();
        init_wstream();
    }

    virtual bool is_open() const        { return _in && _in->is_open(); }

    virtual void flush()
    {
        if(_writebuf.size() > _wlead)
            write_block(true);

        _out->flush();
    }
    virtual void acknowledge (bool eat=false)
    {
    }

    virtual opcd close( bool linger=false )
    {
        flush();
        return 0;
    }

    packstreamlz4() : _LZ4_Data(0), _wlead(0), _rlead(0)
    {}
    packstreamlz4( binstream* bin, binstream* bout )
        : packstream(bin,bout), _LZ4_Data(0), _wlead(0), _rlead(0)
    {}

    ///
    virtual opcd write_raw( const void* p, uints& len )
    {
        if(_outbuf.size() == 0) init_wstream();

        while(_writebuf.size() + len >= BLOCKSIZE)
        {
            uint rem = BLOCKSIZE - _writebuf.size();
            ::memcpy(_writebuf.add(rem), p, rem);
            p = ptr_byteshift(p, rem);
            len -= rem;

            write_block(false);
        }

        if(len) {
            ::memcpy(_writebuf.add(len), p, len);
            len = 0;
        }

        return 0;
    }

    ///
    virtual opcd read_raw( void* p, uints& len )
    {
        return ersNOT_IMPLEMENTED;
    }

protected:

    void init_wstream()
    {
        _writebuf.reserve(BLOCKSIZE, false);
        _outbuf.alloc(LZ4_compressBound(BLOCKSIZE));

        if(_LZ4_Data)
            LZ4_free(_LZ4_Data);
        _LZ4_Data = LZ4_create(_writebuf.ptr());
        _wlead = 0;
    }

    void write_block( bool final )
    {
        int written = LZ4_compress_continue(_LZ4_Data,
            _writebuf.ptr() + _wlead,
            _outbuf.ptr(),
            _writebuf.size() - _wlead);

        if(written)
            _out->xwrite_raw(_outbuf.ptr(), written);

        if(final) {
            _writebuf.reset();
            init_wstream();
        }
        else {
            char* p = LZ4_slideInputBuffer(_LZ4_Data);
            _wlead = p - _writebuf.ptr();
            _writebuf.resize(_wlead);
        }
    }

    void read_block()
    {
        if(_readbuf.size() == 0) {
            _inbuf.reserve(BLOCKSIZE, false);
            _readbuf.alloc(BLOCKSIZE);
            _rlead = 0;
        }

        _inbuf.del(_ilead, _inbuf.size()-_ilead);

        uints ts = BLOCKSIZE - _inbuf.size();
        uints rs = ts;
        _in->read_raw_full(_inbuf.ptre(), rs);
        _inbuf.set_size(BLOCKSIZE - rs);

        int iread = LZ4_decompress_safe_withPrefix64k(
            _inbuf.ptr(), _readbuf.ptr()+_rlead, _inbuf.size(), _readbuf.size() - _rlead);
    }


private:

    dynarray<char> _writebuf, _outbuf;
    dynarray<char> _readbuf, _inbuf;
    void* _LZ4_Data;
    uint _wlead, _rlead, _ilead;
};

COID_NAMESPACE_END

#endif //__COID_COMM_PACKSTREAMLZ4__HEADER_FILE__
