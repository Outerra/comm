
#include "coid/comm/binstream/stlstream.h"
#include "coid/comm/binstream/filestream.h"
#include "coid/comm/metastream/metastream.h"
#include "coid/comm/metastream/fmtstreamcxx.h"

namespace coid {

static void test()
{
    bofstream bof("test_stl");

    std::vector<charstr> x0;
    std::list<charstr> x1;
    std::deque<charstr> x2;
    std::set<charstr> x3;
    std::multiset<charstr> x4;
    std::map<charstr,uint> x5;
    std::multimap<charstr,uint> x6;

    bof << x0 << x1 << x2 << x3 << x4 << x5 << x6;
    bof.close();


    bifstream bif("test_stl");

    bif >> x1 >> x2 >> x3 >> x4 >> x5 >> x6;
    bif.close();



    bof.open("test_stl_meta");
    metastream meta;
    fmtstreamcxx txpo(bof);
    meta.bind_formatting_stream(txpo);

    meta.stream_out(x0,"vector");
    meta.stream_out(x1,"list");
    meta.stream_out(x2,"deque");
    meta.stream_out(x3,"set");
    meta.stream_out(x4,"multiset");
    meta.stream_out(x5,"map");
    meta.stream_out(x6,"multimap");

    bof.close();


    bif.open("test_stl_meta");

    fmtstreamcxx txpi(bif);
    meta.bind_formatting_stream(txpi);

    meta.stream_out(x0,"vector");
    meta.stream_out(x1,"list");
    meta.stream_out(x2,"deque");
    meta.stream_out(x3,"set");
    meta.stream_out(x4,"multiset");
    meta.stream_out(x5,"map");
    meta.stream_out(x6,"multimap");

    bif.close();
}

} //namespace coid
