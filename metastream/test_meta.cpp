
#include "metastream.h"
#include "fmtstreamcxx.h"
//#include "metagen.h"

#ifdef _DEBUG
COID_NAMESPACE_BEGIN

struct FooA
{
    int i;
    float f;

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
            MM(m, "i", s.i)
            MM(m, "f", s.f)
        MSTRUCT_CLOSE(m)
    }
};

struct FooB
{
    int a;
    dynarray<FooA> af;
    dynarray< dynarray<FooA> > aaf;
    dynarray< dynarray< dynarray<FooA> > > aaaf;
    dynarray<int> ai;
    dynarray< dynarray<int> > aai;
    int end;

    friend binstream& operator << (binstream& bin, const FooB& s)
    {   return bin << s.a << s.af << s.aaf << s.aaaf << s.ai << s.aai << s.end;   }
    friend binstream& operator >> (binstream& bin, FooB& s)
    {   return bin >> s.a >> s.af >> s.aaf >> s.aaaf >> s.ai >> s.aai >> s.end;   }
    friend metastream& operator << (metastream& m, const FooB& s)
    {
        MSTRUCT_OPEN(m,"FooB")
            MM(m, "a", s.a)
            MM(m, "af", s.af)
            MM(m, "aaf", s.aaf)
            MM(m, "aaaf", s.aaaf)
            MM(m, "ai", s.ai)
            MM(m, "aai", s.aai)
            MM(m, "end", s.end)
        MSTRUCT_CLOSE(m)
    }
};



static const char* teststr =
"(FooB) {"
"	a = 100"
"	af = ["
"	(FooA) {"
"		i = 1"
"		f = 3.140"
"	}, "
"	(FooA) {"
"		i = 2"
"		f = 4.140"
"	}, "
"	(FooA) {"
"		i = 3"
"		f = 5.140"
"	}]"
"	aaf = [ [ { i=9 f=8.33 }, { i=10 f=4.66 }, { i=11 f=7.66 } ], [] ]"
"   ai = [ 1, 2, 3, 4, 5 ]"
"   aai = [ [1, 2, 3], [4, 5], [6] ]"
"	aaaf = [ [ [ { i=9 f=8.33 }, { i=10 f=4.66 }, { i=11 f=7.66 } ], [] ], [ [ { i=33 f=0.66 } ] ] ]"
"   end = 99"
"}";

////////////////////////////////////////////////////////////////////////////////
void metastream_test()
{
    binstreamconstbuf buf(teststr);
    fmtstreamcxx fmt(buf);

    metastream meta;
    meta.bind_formatting_stream(fmt);

    FooB b;
    meta.stream_in(b);
    meta.stream_acknowledge();

    binstreambuf bof;
    fmt.bind(bof);

    meta.stream_out(b);
    meta.stream_flush();

    charstr x;
    bof.swap(x);
};

COID_NAMESPACE_END
#endif //_DEBUG
