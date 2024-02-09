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
 * Portions created by the Initial Developer are Copyright (C) 2013-2021
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
#include "binstream/binstream.h"
#include "commexception.h"

#include <limits>
#include <type_traits>

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Binary string class. Written values are aligned by their size and specified packing value.
///Read values are returned via fetch() as references into the stream.
class binstring
{
public:

    COIDNEWDELETE(binstring);

    binstring()
    {}

    binstring(binstring&& other) noexcept
        : _tstr(std::move(other._tstr))
        , _offset(other._offset)
        , _packing(other._packing)
    {
        other._offset = 0;
    }

    ///Set the maximum packing alignment
    /// @note resulting alignment is the minimum of this value and alignof(type)
    void set_packing(uint pack) {
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

    binstring& write_fixed_string(const token& tok) {
        ::memcpy(_tstr.add(tok.len()), tok.ptr(), tok.len());
        return *this;
    }

    binstring& write_varstring(const token& tok) {
        uints len = tok.len();
        write_varint(len);
        if (len)
            ::memcpy(_tstr.add(len), tok.ptr(), len);
        return *this;
    }

#ifdef COID_VARIADIC_TEMPLATES
    template<class T, class...Ps>
    T& push(Ps&&... ps) {
        return *new(pad_alloc<T>()) T(std::forward<Ps>(ps)...);
    }
#endif //COID_VARIADIC_TEMPLATES

    template<class T>
    binstring& operator << (const T& v) {
        *pad_alloc<T>() = v;
        return *this;
    }

    ///Write data
    /// @return position in buffer
    template<class T>
    uints write(const T& v) {
        T* p = pad_alloc<T>();
        *p = v;
        return (uint8*)p - _tstr.ptr();
    }

    ///Allocate space for an array of n elements, padding for type T
    /// @return pointer to the allocated buffer
    /// @note for n=0 applies only the padding
    template<class T>
    T* alloc_array(uints n) {
        return pad_alloc<T>(n);
    }

    ///Write padding needed if T was written afterwards
    template<class T>
    uints write_padding() {
        T* p = pad_alloc<T>(0);
        return (uint8*)p - _tstr.ptr();
    }

    template<class T>
    void write_to_offset(uints offset, const T& v) {
        DASSERTN(offset + sizeof(T) <= _tstr.size());
        *(T*)(_tstr.ptr() + offset) = v;
    }

    ///Append string with optionally specified size type (uint8, uint16, uint32 ...)
    template<class SIZE COID_DEFAULT_OPT(uint)>
    binstring& append_string(const token& tok) {
        uints size = write_size<SIZE>(tok.len());
        _tstr.add_bin_from((const uint8*)tok.ptr(), size);
        return *this;
    }

    ///Append string with optionally specified size type (uint8, uint16, uint32 ...)
    template<class SIZE COID_DEFAULT_OPT(uint)>
    binstring& append_string(const char* czstr) {
        token tok(czstr);
        uints size = write_size<SIZE>(tok.len());
        _tstr.add_bin_from((const uint8*)tok.ptr(), size);
        return *this;
    }

    ///Append string with optionally specified size type (uint8, uint16, uint32 ...)
    template<class SIZE COID_DEFAULT_OPT(uint)>
    binstring& append_string(const charstr& str) {
        uints size = write_size<SIZE>(str.len());
        _tstr.add_bin_from((const uint8*)str.ptr(), size);
        return *this;
    }

    ///Add buffer with specified leading alignment
    binstring& append_buffer(const void* p, uints len, uint alignment = 1) {
        uints size = _tstr.size();
        uints align = align_value_up(size, alignment) - size;

        ::memcpy(_tstr.add(align + len) + align, p, len);
        return *this;
    }

    ///Write number as a varint
    template<class T>
    binstring& write_varint(T num) {
        uint8 buf[(8 * sizeof(T)) / 7 + 1];
        uint8* pb = buf;

        while (num & ~0x7f) {
            uint8 b = num & 0xff;
            *pb++ = b | 0x80;
            num >>= 7;
        }
        *pb++ = uint8(num);
        uints len = pb - buf;
        DASSERT(len <= sizeof(buf));

        uints size = _tstr.size();
        uints align = align_value_up(size, _packing < alignof(T) ? _packing : alignof(T)) - size;

        ::memcpy(_tstr.add(align + len) + align, buf, len);
        return *this;
    }

    ///Align the reader offset to the specified alignment
    /// @return true if data are available
    bool align(uint alignment) {
        _offset = align_value_up(_offset, alignment);
        return has_data();
    }

    ///Fetch reference to a typed value from the binary stream
    template<class T>
    const T& fetch() {
        return *seek<std::remove_reference_t<T>>(_offset);
    }

    ///Fetch pointer to a typed array
    template<class T>
    const T* fetch_array(uints n) {
        return seek<std::remove_reference_t<T>>(_offset, n);
    }

    ///Read (copy) data into target variable
    template<class T>
    T& read(T& dst) {
        return dst = fetch<T>();
    }

    ///Read a block of data
    const void* read_buffer(uints size, uint alignment = 1) {
        uints offset = align_value_up(_offset, alignment);
        _offset += size;
        return ptr() + offset;
    }

    ///Fetch google protobuf varint from the binary stream
    template<class T>
    T varint()
    {
        const uint8* buffer = _tstr.ptr() + _offset;

        uint64 result = 0;
        size_t count = 0;
        uint8 b;

        do {
            b = buffer[count];
            result |= uint64(b & 0x7F) << (7 * count);
            ++count;
        }
        while ((b & 0x80) && (count < (sizeof(T) * 8 + 6) / 7));

        _offset += count;

        return std::is_unsigned<T>::value
            ? T(result)
            : T((result >> 1) ^ -int64(result & 1));
    }

    ///Return pointer to data of given type, given a starting offset in buffer
    template<class T>
    T* data(uints offset) {
        return seek<T>(offset);
    }

    ///Fetch string from the binary stream
    template<class SIZE COID_DEFAULT_OPT(uint)>
    token string() {
        const SIZE& size = fetch<SIZE>();
        if (_tstr.size() - _offset < size)
            throw exception("buffer overflow");
        const uint8* p = _tstr.ptr() + _offset;
        _offset += size;
        return token((const char*)p, size);
    }

    ///Fetch string that's preceded by varint size info
    token varstring() {
        uints size = varint<uints>();
        if (_tstr.size() - _offset < size)
            throw exception("buffer overflow");

        const uint8* p = _tstr.ptr() + _offset;
        _offset += size;
        return token((const char*)p, size);
    }

    /// @brief ensure a fixed string follows in the stream
    /// @param str string to check
    /// @return true if found, if false, offset is not advanced
    bool fixed_string(const token& str) {
        if (_tstr.size() - _offset < str.len())
            return false;
        if (str != token((const char*)_tstr.ptr() + _offset, str.len()))
            return false;
        _offset += str.len();
        return true;
    }

    ///Read/append data from binstream
    /// @return size read
    uints load_from_binstream(binstream& bin, uints datasize = UMAXS)
    {
        uints old = _tstr.size();
        uints n = 0;

        while (1)
        {
            static const uints packet = 4096;
            const uints len = datasize < packet ? datasize : packet;
            uint8* ptr = _tstr.add(len);

            uints toread = len;
            opcd e = bin.read_raw_full(ptr, toread);

            uints d = len - toread;
            datasize -= d;
            n += d;

            if (e != NOERR || toread > 0 || datasize == 0)
                break;
        }

        _tstr.resize(old + n);
        return n;
    }


    friend void swap(binstring& a, binstring& b) {
        a._tstr.swap(b._tstr);
    }

    ///Swap strings
    void swap(binstring& ref) {
        _tstr.swap(ref._tstr);
    }

    template<class COUNT>
    binstring& swap(dynarray<char, COUNT>& ref)
    {
        _tstr.swap(reinterpret_cast<dynarray<uint8, COUNT>&>(ref));
        _offset = 0;
        return *this;
    }

    template<class COUNT>
    binstring& swap(dynarray<uchar, COUNT>& ref)
    {
        _tstr.swap(ref);
        _offset = 0;
        return *this;
    }

    binstring& swap(charstr& str, bool removetermzero)
    {
        swap(str.dynarray_ref());

        if (removetermzero) {
            if (_tstr.size() > 0 && *_tstr.last() == 0)
                _tstr.resize(-1);
            if (str.lent() > 0 && *str.dynarray_ref().last() != 0)
                str.dynarray_ref().push(0);
        }
        return *this;
    }

    dynarray<uint8>& get_buf() { return _tstr; }
    const dynarray<uint8>& get_buf() const { return _tstr; }


    ///Cut to specified length, negative numbers cut abs(len) from the end
    binstring& resize(ints len) {
        _tstr.resize(len);
        return *this;
    }


    ///Allocate buffer and reset the reading offset
    uint8* alloc(uints len) {
        uint8* p = _tstr.alloc(len);
        _offset = 0;
        return p;
    }

    /// @return pointer to the string beginning
    const uint8* ptr() const { return _tstr.ptr(); }

    /// @return pointer past the string end
    const uint8* ptre() const { return _tstr.ptre(); }


    /// @return total binstring length
    uints len() const { return _tstr.size(); }

    /// @return the remaining (unread) length
    uints remaining_len() const { return _tstr.size() - _offset; }

    /// @return true if binstring still has data available for reading
    bool has_data() const { return _offset < _tstr.size(); }

    /// @return current reading offset
    uints offset() const { return _offset; }

    ///Set the reading offset
    uints set_offset(uints offs) {
        if (offs >= _tstr.size())
            offs = _tstr.size();

        return _offset = offs;
    }

    ///Set string to empty and discard the memory
    void free() { _tstr.discard(); }

    ///Reserve memory for string
    uint8* reserve(uints len) { return _tstr.reserve(len, true); }

    ///Reset string to empty but keep the memory
    void reset() {
        _tstr.reset();
        _offset = 0;
    }

    ~binstring() {}

protected:

    template<class S>
    uints write_size(uints size) {
        static_assert(std::is_unsigned<S>::value, "unsigned type expected");
        DASSERTN(size <= std::numeric_limits<S>::max());
        size = std::min(size, (uints)std::numeric_limits<S>::max());
        *pad_alloc<S>() = S(size);
        return size;
    }

    template<class T>
    T* pad_alloc(uints n = 1) {
        uints size = _tstr.size();
        uints align = align_value_up(size, _packing < alignof(T) ? _packing : alignof(T)) - size;

        return reinterpret_cast<T*>(_tstr.add(align + n * sizeof(T)) + align);
    }

    template<class T>
    T* seek(uints& offset, uints count = 1) {
        uints off = align_value_up(offset, _packing < alignof(T) ? _packing : alignof(T));
        uints size = count * sizeof(T);
        if (off + size > _tstr.size())
            throw exception("error reading buffer");

        T* p = reinterpret_cast<T*>(_tstr.ptr() + off);
        offset = off + size;
        return p;
    }

protected:

    dynarray<uint8> _tstr;
    uints _offset = 0;
    uint _packing = 1;
};


////////////////////////////////////////////////////////////////////////////////

COID_NAMESPACE_END

#endif //__COID_COMM_BINSTRING__HEADER_FILE__
