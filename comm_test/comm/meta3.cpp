
//#include "comm/binstream/filestream.h"
#include "../binstream/binstreambuf.h"
#include "../metastream/metastream.h"
#include "../metastream/fmtstreamjson.h"
#include "../ref.h"
#include "../metastream/metagen.h"

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

    friend metastream& operator || (metastream& m, FooA& s)
    {
        return m.compound("FooA", [&]()
        {
            m.member("i", s.i, 4);
            m.member("f", s.f, 3.3f);
        });
    }
};

////////////////////////////////////////////////////////////////////////////////

struct float3
{
    float x, y, z;
};

struct float4
{
    float x, y, z, w;
};

struct double3
{
    double x, y, z;
};

COID_METABIN_OP3D(float3,x,y,z,0.0f,0.0f,0.0f)
COID_METABIN_OP3D(double3,x,y,z,0.0,0.0,0.0)
COID_METABIN_OP4D(float4,x,y,z,w,0.0f,0.0f,0.0f,0.0f)

struct rec
{
    double3 pos;
    float4 rot;
    float3 dir;
    float weight;
    float speed;

    friend metastream& operator || (metastream& m, rec& w)
    {
        return m.compound("flight_path_waypoint", [&]()
        {
            m.member("pos", w.pos);
            m.member("rot", w.rot);
            m.member_obsolete<float>("speed");
            m.member("dir", w.dir);
            m.member("weight", w.weight, 1);
            m.member("speed", w.speed, 10.0f);
        });
    }
};


struct root
{
    dynarray<rec> records;
};

COID_METABIN_OP1(root,records)


static const char* text = "\
{\
	\"records\" : [\
	{\
		\"pos\" : \
		{\
			\"x\" : -1359985.5, \
			\"y\" : -7982546.2, \
			\"z\" : 1210054.45\
		}, \
		\"speed\" : 13888.8906, \
		\"rot\" : \
		{\
			\"x\" : .973808, \
			\"y\" : -.082526, \
			\"z\" : -.015322, \
			\"w\" : .211312\
		}, \
		\"dir\" : \
		{\
			\"x\" : 898.872803, \
			\"y\" : 5680.9126, \
			\"z\" : 12642.0156\
		}, \
		\"weight\" : 1.0\
	}, \
	{\
		\"pos\" : \
		{\
			\"x\" : -1275636.6, \
			\"y\" : -7559190.6, \
			\"z\" : 1357102.11\
		}, \
		\"speed\" : 125444.555, \
		\"rot\" : \
		{\
			\"x\" : .811473, \
			\"y\" : -.052306, \
			\"z\" : -.076776, \
			\"w\" : .576959\
		}, \
		\"dir\" : \
		{\
			\"x\" : 23202.2227, \
			\"y\" : 116455.305, \
			\"z\" : 40449.4023\
		}, \
		\"weight\" : 1.0\
	}\
    ]\
}";

static void metastream_test3x()
{
    binstreamconstbuf bc(text);
    fmtstreamjson fmt(bc, false);
    metastream meta(fmt);

    root r;
    meta.stream_in(r);
}

////////////////////////////////////////////////////////////////////////////////

struct upper {
    charstr norm;
    charstr spec;

    friend metastream& operator || (metastream& m, upper& w)
    {
        return m.compound_type(w, [&]()
        {
            m.member("norm", w.norm);
            m.member("spec", w.spec);
        });
    }

};

static void fmtstreamjson_test()
{
    binstreamconstbuf bc("{ norm : \"normal text\", spec : \\\"escaped text\n\" }");
    binstreambuf buf;
    fmtstreamjson fmt(bc, true);
    metastream meta(fmt);

    upper r;
    meta.stream_in(r);

    fmt.bind(buf);

    meta.stream_out(r);

    token tok = buf;
}

////////////////////////////////////////////////////////////////////////////////

struct container {
    char bordel[2];
    int* p;
    int16 sajrajt;

    friend metastream& operator || (metastream& m, container& c) {
        return m.member_container_type(c, &c.p, true,
            [](const void* a) -> const void* { return static_cast<const container*>(a)->p; },
            [](const void* a) -> uints { return UMAXS; },
            [](void* a, uints& i) -> void* { return static_cast<container*>(a)->p + i++; },
            [](const void* a, uints& i) -> const void* { return static_cast<const container*>(a)->p + i++; }
        );
    }
};

struct aaa {
    int x;
    char* salama;
    int16 bordel[7];

    container c;
    const char* string;

    dynarray<token> stuff;

    friend metastream& operator || (metastream& m, aaa& v) {
        return m.compound_type(v, [&]() {
            m.member("x", v.x);
            m.member("salama", v.salama);
            m.member("bordel", v.bordel);
            m.member("c", v.c);
            m.member("str", v.string);
            m.member("stuff", v.stuff);
        });
    }
};

void metastream_test4()
{
    metastream m;
    auto a = m.get_type_desc<dynarray<token>>();
    auto x = m.get_type_desc<aaa>();

    //DASSERT(0);
}

////////////////////////////////////////////////////////////////////////////////

struct testmtg
{
    dynarray<charstr> as;
    dynarray<int> ar;
    charstr name;
    token fame;
    int k;

    friend metastream& operator || (metastream& m, testmtg& p) {
        return m.compound_type(p, [&]() {
            m.member("as", p.as);
            m.member("ar", p.ar);
            m.member("name", p.name);
            m.member("k", p.k);
        });

        /*return m.compound_type_stream_as_compound(p, [&]() {
            //reflection
            m.member("as", p.as);
            m.member("ar", p.ar);
            m.member("name", p.name);
            m.member("k", p.k);
        }, [&]() {
            //stream
            int x;
            coid::charstr y;

            if (m.stream_writing()) {
                //prepare x, y
            }

            m.member("x", x);
            m.member("y", y);

            if (m.stream_reading()) {
                //do something with x, y
            }
        });*/
    }
};


static void metagen_test()
{
    testmtg t;
    t.as.push("abc");
    t.as.push("def");
    t.ar.push(1);
    t.ar.push(3);
    t.name = "jozo";
    t.k = 47;

    const token mtg = "\
test\n\
numba $k$\n\
$[as rest=\" \"]$$@value$$[/as]$\n\
$[ar]$$@value$$[/ar]$\n\
$[ar final=\"$@order$\"]$$[/ar]$";

    metagen m;
    m.parse(mtg);

    binstreambuf dst;

    m.generate(t, dst);
    token res = dst;
}



struct as_type
{
    struct something {
        charstr blah;
        int fooi;

        bool operator == (const something& s) const {
            return blah == s.blah && fooi == s.fooi;
        }

        friend metastream& operator || (metastream& m, something& p) {
            return m.compound_type_stream_as_type<coid::charstr>(p,
                [&]() {
                m.member("blah", p.blah);
                m.member("fooi", p.fooi);
            },
                [](something& p, coid::charstr&& v) {
                token t = v;
                token n = t.cut_right_back(':');
                p.blah.takeover(v);
                p.blah.resize(t.len());
                p.fooi = n.toint();
            },
                [](something& p) -> coid::charstr {
                return p.blah + ':' + p.fooi;
            });
        }
    };

    dynarray<something> aaa;
    dynarray<something> bbb;

    something* s = 0;

    friend metastream& operator || (metastream& m, as_type& p) {
        return m.compound_type(p, [&]() {
            m.member("aaa", p.aaa, dynarray<something>(), false);
            m.member("bbb", p.bbb, dynarray<something>(), false);
        });
    }
};

static void test_as_type()
{
    const char* str = "aaa = [\"jozo:1\", \"fero:2\"]";

    binstreamconstbuf cbuf(str);
    fmtstreamcxx fmt(cbuf);
    metastream meta(fmt);

    as_type tat;
    tat.s = new as_type::something;

    meta.xstream_in(tat);
}

////////////////////////////////////////////////////////////////////////////////

struct FooCCC
{
    float f;

    FooCCC() {}
    FooCCC(float f) { set(f); }

    void set(float f)
    {
        this->f = f;
    }

    friend metastream& operator || (metastream& m, FooCCC& s)
    {
        return m.compound_type(s, [&]()
        {
            m.member("f", s.f);
        });
    }
};

struct FooAAA
{
    int i;
    FooCCC c;

    FooAAA() {}
    FooAAA(int i, float f) { set(i, f); }
    bool operator==(const FooAAA& other) { return other.i == i && other.c.f == c.f; }

    void set(int i, float f)
    {
        this->i = i;
        this->c.f = f;
    }

    friend metastream& operator || (metastream& m, FooAAA& s)
    {
        return m.compound_type(s, [&]()
        {
            m.member("i", s.i);
            m.member("c", s.c);
        });
    }
};

////////////////////////////////////////////////////////////////////////////////

struct FooBBB
{
    bool b = false;
    bool c = false;
    FooAAA a;

    FooBBB() = default;
    FooBBB(int i, float f, bool b) { set(i, f, b); }


    void set(int i, float f, bool b)
    {
        this->b = b;
        a = FooAAA(i, f);
    }

    friend metastream& operator || (metastream& m, FooBBB& s)
    {
        return m.compound_type(s, [&]()
        {
            m.member_optional("b", s.b, false);
            m.member_optional("c", s.c, false);
            m.member_optional_nowritedef("a", s.a, FooAAA());
        });
    }

};

void test_nested_compound()
{
    const char* test_input =
        "[{a: {i:2, c: {f : 3.0}} , b: true, c: true},  {a: {i:2} , b: true, c: true}]";

    binstreamconstbuf txt(test_input);
    fmtstreamjson fmt(txt, false);
    metastream meta(fmt);

    coid::dynarray<FooBBB> bs;

    meta.stream_in(bs);
    meta.stream_acknowledge();

}

////////////////////////////////////////////////////////////////////////////////
void metastream_test3()
{
    metagen_test();

    metastream_test3x();

    fmtstreamjson_test();

    test_as_type();

    test_nested_compound();
/*
    dynarray<ref<FooA>> ar;
    ar.add()->create(new FooA(1, 2));

    dynarray<ref<FooA>>::dynarray_binstream_container bc(ar);
    binstream_dereferencing_containerT<FooA,uints> dc(bc);

    binstreambuf txt;
    fmtstreamjson fmt(txt);
    metastream meta(fmt);

    meta.stream_array_out(dc);
    meta.stream_flush();
    //txt.swap(json);*/
};
