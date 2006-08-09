
#include "coid/comm/binstream/stlstream.h"
#include "coid/comm/binstream/filestream.h"
#include "coid/comm/metastream/metastream.h"
#include "coid/comm/metastream/fmtstreamcxx.h"

namespace coid {

static void test()
{
    //binstream out and in test

    bofstream bof("test_stl");

    const char* t = "test";
    std::vector<std::string> x0;
    std::list<std::string> x1;
    std::deque<std::string> x2;
    std::set<std::string> x3;
    std::multiset<std::string> x4;
    std::map<std::string,uint> x5;
    std::multimap<std::string,uint> x6;

    bof << t << x0 << x1 << x2 << x3 << x4 << x5 << x6;
    bof.close();


    bifstream bif("test_stl");

    bif >> t >> x1 >> x2 >> x3 >> x4 >> x5 >> x6;
    bif.close();

    ::free((void*)t);

    
    //metastream out and in test

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
