
//#include "comm/binstream/filestream.h"
#include "comm/binstream/binstreambuf.h"
#include "comm/metastream/metastream.h"
#include "comm/metastream/fmtstreamjson.h"
#include "comm/ref.h"
//#include "metagen.h"

using namespace coid;

struct FooA
{
    int i;
    float f;

    FooA() {}
    FooA( int i, float f ) { set(i,f); }

    void set( int i, float f )
    {
        this->i = i;
        this->f = f;
    }

    friend binstream& operator << (binstream& bin, const FooA& s)
    {   return bin << s.i << s.f;   }
    friend binstream& operator >> (binstream& bin, FooA& s)
    {   return bin >> s.i >> s.f;   }
    friend metastream& operator << (metastream& m, const FooA& s)
    {
        MSTRUCT_OPEN(m,"FooA")
            MMD(m, "i", s.i, 4)
            MMD(m, "f", s.f, 3.3f)
        MSTRUCT_CLOSE(m)
    }
};


////////////////////////////////////////////////////////////////////////////////
void metastream_test3()
{
    dynarray<ref<FooA>> ar;
    ar.add()->create(new FooA(1, 2));

    dynarray<ref<FooA>>::dynarray_binstream_container bc =
        ar.get_container_to_read();
    binstream_dereferencing_containerT<FooA,uints>
        dc(bc);

    binstreambuf txt;
    fmtstreamjson fmt(txt);
    metastream meta(fmt);

    meta.stream_array_out(dc);
    meta.stream_flush();
    //txt.swap(json);
};
