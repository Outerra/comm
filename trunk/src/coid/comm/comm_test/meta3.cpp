
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

	friend binstream& operator << (binstream& bin, const rec& w) {
		return bin << w.pos << w.rot << w.dir << w.speed << w.weight;
	}

	friend binstream& operator >> (binstream& bin, rec& w) {
		return bin >> w.pos >> w.rot >> w.dir >> w.speed >> w.weight;
	}

	friend metastream& operator << (metastream& m, const rec& w)
	{
		MSTRUCT_OPEN(m, "flight_path_waypoint")
			MM(m, "pos", w.pos)
			MM(m, "rot", w.rot)
            //MMT_OBSOLETE(m, "speed", float)
            MM(m, "dir", w.dir)
            MM(m, "speed", w.speed)
			MM(m, "weight", w.weight)
		MSTRUCT_CLOSE(m)
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
    fmtstreamjson fmt(bc);
    metastream meta(fmt);

    root r;
    meta.stream_in(r);
}

////////////////////////////////////////////////////////////////////////////////
void metastream_test3()
{
    metastream_test3x();

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
