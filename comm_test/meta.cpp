
#include "comm/binstream/filestream.h"
#include "comm/binstream/binstreambuf.h"
#include "comm/metastream/metastream.h"
#include "comm/metastream/fmtstreamcxx.h"
#include "comm/metastream/fmtstreamxml2.h"
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
            MMD(m, "f", s.f, 3.3f)
        MSTRUCT_CLOSE(m)
    }
};

struct FooAA
{
    charstr text;
    int j;
    FooA fa;

    void set( int j, int i, float f )
    {
        this->j = j;
        fa.i = i;
        fa.f = f;
    }

    friend binstream& operator << (binstream& bin, const FooAA& s)
    {   return bin << s.text << s.j << s.fa;   }
    friend binstream& operator >> (binstream& bin, FooAA& s)
    {   return bin >> s.text >> s.j >> s.fa;   }
    friend metastream& operator << (metastream& m, const FooAA& s)
    {
        MSTRUCT_OPEN(m,"FooAA")
            MMD(m, "text", s.text, "def")
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

    FooAA* pfo;
    int end;


    FooB() : pfo(0)
    {}

    friend binstream& operator << (binstream& bin, const FooB& s)
    {   return bin << s.a << s.b << s.fx << s.af << s.aaf << s.aaaf << s.ai << s.aai << pointer(s.pfo) << s.end;   }
    friend binstream& operator >> (binstream& bin, FooB& s)
    {   return bin >> s.a >> s.b >> s.fx >> s.af >> s.aaf >> s.aaaf >> s.ai >> s.aai >> pointer(s.pfo) >> s.end;   }
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
            MMP(m, "p", s.pfo)
            MM(m, "end", s.end)
        MSTRUCT_CLOSE(m)
    }
};



static const char* teststr =
"a = 100,\n"
"b = 200,\n"
"fx = { j=1, fa={i=-1, f=-.77} },\n"
"af = [  { j=10, fa={i=1, f=3.140}},\n"
"        { j=11, fa={i=2, f=4.140}},\n"
"        { j=12, fa={i=3, f=5.140}} ],\n"
"aaf = [ [ { j=20, fa={i=9, f=8.33} }, { j=21, fa={i=10, f=4.66} }, { j=22, fa={i=11, f=7.66} } ], [] ],\n"
"aaaf = [ [ [ { text=\"\", j=30, fa={i=9, f=8.33} }, { j=31, fa={i=10, f=4.66} }, { j=32, fa={i=11, f=7.66} } ], [] ], [ [ { j=33, fa={i=33, f=0.66} } ] ] ],\n"
"ai = [ 1, 2, 3, 4, 5 ],\n"
"aai = [ [1, 2, 3], [4, 5], [6] ],\n"
"end = 99\n"
;

static const char* teststr1 =
"a = 1,\n"
"fx = { j=1, fa={i=-1, f=-.77} },\n"
"b = 200,\n"
"af = [  { text=\"jano\", j=10 },\n"
"        { fa={i=2, f=4.140}, j=11, text=\"fero\" },\n"
"        { text=\"jozo\", j=12, fa={i=3, f=5.140} } ],\n"
"aaf = [ [ { j=20, fa={i=9, f=8.33} }, { j=21, fa={i=10, f=4.66} }, { j=22, fa={i=11, f=7.66} } ], [] ],\n"
"aaaf = [ [ [ { j=30, fa={i=9, f=8.33} }, { j=31, fa={i=10, f=4.66} }, { j=32, fa={i=11, f=7.66} } ], [] ], [ [ { j=33, fa={i=33, f=0.66} } ] ] ],\n"
"ai = [ 1, 2, 3, 4, 5 ],\n"
"aai = [ [1, 2, 3], [4, 5], [6] ],\n"
"end = 99,\n"
//"p = { j=66, fa={i=6, f=0.998} }\n"
//",a = 100\n"
;

static const char* textxml2 =
"<root xmlns:xsd='http://www.w3.org/2001/XMLSchema'>"
"<a>1</a><b>200</b><fx>\r\n"
//"<text>def</text><j>1</j><fa><i>-1</i><f>-.770</f></fa></fx><af><FooAA>\r\n"
"<text>def</text><j>1</j><fa i='1'/></fx><af><FooAA>\r\n"
"<text>jano</text><j>10</j><fa><i>47</i><f>47.470</f></fa></FooAA><FooAA>\r\n"
"<text>fero</text><j>11</j><fa><i>2</i><f>4.140</f></fa></FooAA><FooAA>\r\n"
"<text>jozo</text><j>12</j><fa><i>3</i><f>5.140</f></fa></FooAA></af><aaf>\r\n"
"<item><FooAA><text>def</text><j>20</j><fa><i>9</i><f>8.330</f></fa>\r\n"
"</FooAA><FooAA><text>def</text><j>21</j><fa><i>10</i><f>4.660</f></fa>\r\n"
"</FooAA><FooAA><text>def</text><j>22</j><fa><i>11</i><f>7.660</f></fa>\r\n"
"</FooAA></item><item></item></aaf><aaaf><item><item><FooAA><text>def</text>\r"
"\n<j>30</j><fa><i>9</i><f>8.330</f></fa></FooAA><FooAA><text>def</text>\r\n"
"<j>31</j><fa><i>10</i><f>4.660</f></fa></FooAA><FooAA><text>def</text>\r\n"
"<j>32</j><fa><i>11</i><f>7.660</f></fa></FooAA></item><item></item></item>\r\n"
"<item><item><FooAA><text>def</text><j>33</j><fa><i>33</i><f>.660</f></fa>\r\n"
"</FooAA></item></item></aaaf><ai><xsd:int>1</xsd:int><xsd:int>2</xsd:int>\r\n"
"<xsd:int>3</xsd:int><xsd:int>4</xsd:int><xsd:int>5</xsd:int></ai><aai>\r\n"
"<item><xsd:int>1</xsd:int><xsd:int>2</xsd:int><xsd:int>3</xsd:int></item>\r\n"
"<item><xsd:int>4</xsd:int><xsd:int>5</xsd:int></item><item>\r\n"
"<xsd:int>6</xsd:int></item></aai><end>99</end></root>";

////////////////////////////////////////////////////////////////////////////////
void metastream_test()
{
    binstreamconstbuf buf(teststr);
    fmtstreamcxx fmt(buf);
    metastream meta(fmt);

    FooB b;
    meta.stream_in(b);
    meta.stream_acknowledge();

    bofstream bof("meta.test");
    fmt.bind(bof);

    meta.stream_out(b);
    meta.stream_flush();

    bof.close();
    bof.open("meta-xml2.test");

    fmtstreamxml2 fmx(bof);
    meta.bind_formatting_stream(fmx);
    meta.stream_out(b);
    meta.stream_flush();

    FooB bi;
    buf.set(textxml2);
    fmx.bind(buf);
    meta.stream_in(bi);
    meta.stream_acknowledge();
};

COID_NAMESPACE_END

