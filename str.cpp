
#define _CRT_SECURE_NO_WARNINGS

#include "str.h"
#include "binstream/binstream.h"
#include "binstream/container_dynarray.h"
#include "metastream/metastream.h"

#include <cstdio>

namespace coid {

////////////////////////////////////////////////////////////////////////////////
binstream& operator >> (binstream& bin, charstr& x)
{
    auto& a = x.dynarray_ref();
    a.reset();
    dynarray_binstream_container<char, uint> c(a);

    bin.xread_array(c);
    if (a.size())
        *a.add() = 0;
    return bin;
}

binstream& charstr::append_from_stream(binstream& bin)
{
    dynarray_binstream_container<char, uint> c(_tstr);

    bin.xread_array(c);
    if (_tstr.size())
        *_tstr.add() = 0;
    return bin;
}

binstream& operator << (binstream& out, const charstr& x)
{
    return out.xwrite_token(x.ptr(), x.len());
}

binstream& operator << (binstream& out, const token& x)
{
    return out.xwrite_token(x);
}

binstream& operator >> (binstream& out, token& x)
{
    //this should not be called ever
    RASSERT(0);
    return out;
}

////////////////////////////////////////////////////////////////////////////////
opcd binstream::read_key(charstr& key, int kmember, const token& expected_key)
{
    if (kmember > 0) {
        opcd e = read_separator();
        if (e != NOERR) return e;
    }

    key.reset();
    dynarray_binstream_container<bstype::key, uint> c(reinterpret_cast<dynarray<bstype::key, uint>&>(key.dynarray_ref()));

    opcd e = read_array(c);
    if (key.lent())
        key.appendn_uninitialized(1);

    return e;
}




////////////////////////////////////////////////////////////////////////////////
bool charstr::append_from_file(const token& path)
{
    zstring zpath = path;

    return append_from_file(zpath.c_str());
}

////////////////////////////////////////////////////////////////////////////////
bool charstr::append_from_file(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (fp) {
        constexpr int bufsize = 4096;
        size_t rs, total = 0, old = len();

        do {
            char* dst = alloc_append_buf(bufsize);
            rs = fread(dst, 1, bufsize, fp);
            total += rs;
        }
        while (rs == bufsize);

        resize(old + total);
        return true;
    }

    return false;
}

} //namespace coid
