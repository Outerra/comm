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
 * Outerra.
 * Portions created by the Initial Developer are Copyright (C) 2013
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

#ifndef __COID_COMM_BINSTRING__HEADER_FILE__
#define __COID_COMM_BINSTRING__HEADER_FILE__

#include "namespace.h"

#include "token.h"
#include "str.h"
#include "dynarray.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Binary string class. Written values are properly aligned.
///Read values are returned via fetch() as references into the stream.
class binstring
{
public:

    COIDNEWDELETE(binstring);

    binstring()
        : _offset(0), _packing(UMAXS)
    {}

    ///Set the maximum packing alignment
    //@note resulting alignment is the minimum of this value and __alignof(type)
    void set_packing( uints pack ) {
        _packing = pack;
    }


    ///
    binstring& operator << (const token& tok) {
        *this << tok.len();
        _tstr.add_bin_from((const uint8*)tok.ptr(), tok.len());
        return *this;
    }

    binstring& operator << (const char* czstr) {
        token tok(czstr);
        *this << tok.len();
        _tstr.add_bin_from((const uint8*)tok.ptr(), tok.len());
        return *this;
    }

    binstring& operator << (const charstr& str) {
        *this << str.len();
        _tstr.add_bin_from((const uint8*)str.ptr(), str.len());
        return *this;
    }

    template<class T>
    binstring& operator << (const T& v) {
        *pad_alloc<T>() = v;
        return *this;
    }

    ///Add buffer with specified leading alignment
    binstring& append( uint alignment, const void* p, uints len ) {
        uints size = _tstr.size();
        uints align = align_offset(size, alignment) - size;

        ::memcpy(_tstr.add(align + len) + align, p, len);
        return *this;
    }

    ///Align the reader offset to the specified alignment
    //@return true if data are available
    bool align( uint alignment ) {
        _offset = align_offset(_offset, alignment);
        return has_data();
    }

    ///Fetch reference to a typed value from the binary stream
    template<class T>
    const T& fetch() {
        return *seek<T>(_offset);
    }

    ///Return position of data given starting offset in buffer
    template<class T>
    T* data( uints offset ) {
        return seek<T>(offset);
    }

    ///Fetch string from the binary stream
    token string() {
        const uints& size = fetch<uints>();
        if(_tstr.size()-_offset < size)
            throw exception("buffer overflow");
        const uint8* p = _tstr.ptr() + _offset;
        _offset += size;
        return token((const char*)p, size);
    }


    ///Swap strings
    void swap( binstring& ref )
    {
        _tstr.swap( ref._tstr );
    }

    template<class COUNT>
    binstring& swap( dynarray<char,COUNT>& ref )
    {
        _tstr.swap(ref);
        return *this;
    }

    template<class COUNT>
    binstring& swap( dynarray<uchar,COUNT>& ref )
    {
        uchar* p = ref.ptr();
        ref.set_dynarray_conforming_ptr(_tstr.ptr());
        _tstr.set_dynarray_conforming_ptr(p);

        return *this;
    }


    ///Cut to specified length, negative numbers cut abs(len) from the end
    binstring& resize( ints len ) {
        _tstr.resize(len);
        return *this;
    }


    ///Allocate buffer and reset the reading offset
    uint8* alloc( uints len ) {
        uint8* p = _tstr.alloc(len);
        _offset = 0;
        return p;
    }
    
    //@return pointer to the string beginning
    const uint8* ptr() const            { return _tstr.ptr(); }

    //@return pointer past the string end
    const uint8* ptre() const           { return _tstr.ptr() + len(); }


    ///String length
    uints len() const                   { return _tstr.size(); }

    //@return true if binstring still has data available for reading
    bool has_data() const               { return _offset < _tstr.size(); }

    //@return current reading offset
    uints offset() const                { return _offset; }

    ///Set the reading offset
    uints set_offset( uints offs ) {
        if(offs >= _tstr.size())
            offs = _tstr.size();

        return _offset = offs;
    }

    ///Set string to empty and discard the memory
    void free()                         { _tstr.discard(); }

    ///Reserve memory for string
    uint8* reserve( uints len )         { return _tstr.reserve(len, true); }

    ///Reset string to empty but keep the memory
    void reset() {
        _tstr.reset();
        _offset = 0;
    }

    ~binstring() {}

protected:

    template<class T>
    T* pad_alloc() {
        uints size = _tstr.size();
        uints align = align_offset(size, _packing<__alignof(T) ? _packing : __alignof(T)) - size;

        return reinterpret_cast<T*>(_tstr.add(align + sizeof(T)) + align);
    }

    template<class T>
    T* seek( uints& offset ) {
        uints off = align_offset(offset, _packing<__alignof(T) ? _packing : __alignof(T));
        if(off+sizeof(T) > _tstr.size())
            throw exception("error reading buffer");

        T* p = reinterpret_cast<T*>(_tstr.ptr() + off);
        offset = off + sizeof(T);
        return p;
    }

protected:

    dynarray<uint8> _tstr;
    uints _offset;
    uint _packing;
};


////////////////////////////////////////////////////////////////////////////////

COID_NAMESPACE_END

#endif //__COID_COMM_BINSTRING__HEADER_FILE__
