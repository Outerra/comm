
#include "coid/comm/binstream/filestream.h"
#include "coid/comm/metastream/metastream.h"
#include "coid/comm/metastream/fmtstreamcxx.h"
//#include "metagen.h"

COID_NAMESPACE_BEGIN

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
            MM(m, "f", s.f)
        MSTRUCT_CLOSE(m)
    }
};

struct FooAA
{
    int j;
    FooA fa;

    void set( int j, int i, float f )
    {
        this->j = j;
        fa.i = i;
        fa.f = f;
    }

    friend binstream& operator << (binstream& bin, const FooAA& s)
    {   return bin << s.j << s.fa;   }
    friend binstream& operator >> (binstream& bin, FooAA& s)
    {   return bin >> s.j >> s.fa;   }
    friend metastream& operator << (metastream& m, const FooAA& s)
    {
        MSTRUCT_OPEN(m,"FooAA")
            MM(m, "j", s.j)
            MMD(m, "fa", s.fa, FooA(47, 47.47f) )
        MSTRUCT_CLOSE(m)
    }
};

struct FooB
{
    int a,b;
    FooAA fx;
    dynarray<FooAA> af;
    dynarray< dynarray<FooAA> > aaf;
    dynarray< dynarray< dynarray<FooAA> > > aaaf;
    dynarray<int> ai;
    dynarray< dynarray<int> > aai;
    int end;

    friend binstream& operator << (binstream& bin, const FooB& s)
    {   return bin << s.a << s.b << s.fx << s.af << s.aaf << s.aaaf << s.ai << s.aai << s.end;   }
    friend binstream& operator >> (binstream& bin, FooB& s)
    {   return bin >> s.a >> s.b >> s.fx >> s.af >> s.aaf >> s.aaaf >> s.ai >> s.aai >> s.end;   }
    friend metastream& operator << (metastream& m, const FooB& s)
    {
        MSTRUCT_OPEN(m,"FooB")
            MMD(m, "a", s.a, 8)
            MM(m, "b", s.b)
            MM(m, "fx", s.fx)
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
"a = 100\n"
"b = 200\n"
"fx = { j=1 fa={i=-1 f=-.77} }\n"
"af = [  { j=10 fa={i=1 f=3.140}},\n"
"        { j=11 fa={i=2 f=4.140}},\n"
"        { j=12 fa={i=3 f=5.140}} ]\n"
"aaf = [ [ { j=20 fa={i=9 f=8.33} }, { j=21 fa={i=10 f=4.66} }, { j=22 fa={i=11 f=7.66} } ], [] ]\n"
"aaaf = [ [ [ { j=30 fa={i=9 f=8.33} }, { j=31 fa={i=10 f=4.66} }, { j=32 fa={i=11 f=7.66} } ], [] ], [ [ { j=33 fa={i=33 f=0.66} } ] ] ]\n"
"ai = [ 1, 2, 3, 4, 5 ]\n"
"aai = [ [1, 2, 3], [4, 5], [6] ]\n"
"end = 99\n"
"}";

static const char* teststr1 =
"b = 200\n"
"fx = { j=1 fa={i=-1 f=-.77} }\n"
"af = [  { j=10 },\n"
"        { j=11 fa={i=2 f=4.140}},\n"
"        { j=12 fa={i=3 f=5.140}} ]\n"
"aaf = [ [ { j=20 fa={i=9 f=8.33} }, { j=21 fa={i=10 f=4.66} }, { j=22 fa={i=11 f=7.66} } ], [] ]\n"
"aaaf = [ [ [ { j=30 fa={i=9 f=8.33} }, { j=31 fa={i=10 f=4.66} }, { j=32 fa={i=11 f=7.66} } ], [] ], [ [ { j=33 fa={i=33 f=0.66} } ] ] ]\n"
"ai = [ 1, 2, 3, 4, 5 ]\n"
"aai = [ [1, 2, 3], [4, 5], [6] ]\n"
"end = 99\n"
//"a = 100\n"
"}";

////////////////////////////////////////////////////////////////////////////////
void metastream_test()
{
    binstreamconstbuf buf(teststr1);
    fmtstreamcxx fmt(buf);
    metastream meta(fmt);

    FooB b;
    meta.stream_in(b);
    meta.stream_acknowledge();

    bofstream bof("meta.test");
    fmt.bind(bof);

    meta.stream_out(b);
    meta.stream_flush();
};

COID_NAMESPACE_END

