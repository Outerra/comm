
#include "comm/binstream/stlstream.h"
#include "comm/binstream/filestream.h"
#include "comm/metastream/metastream.h"
#include "comm/metastream/fmtstreamcxx.h"
#include "comm/metastream/fmtstreamxml2.h"

#include <sstream>

namespace coid {

struct STLMIX {
    std::vector<std::string> x0;
    std::list<std::string> x1;
    std::deque<std::string> x2;
    std::set<std::string> x3;
    std::multiset<std::string> x4;
    std::map<std::string,uint> x5;
    std::multimap<std::string,uint> x6;

    inline friend binstream& operator << (binstream& bin, const STLMIX& x)
    {   return bin << x.x0 << x.x1 << x.x2 << x.x3 << x.x4 << x.x5 << x.x6; }
    
    inline friend binstream& operator >> (binstream& bin, STLMIX& x)
    {   return bin >> x.x0 >> x.x1 >> x.x2 >> x.x3 >> x.x4 >> x.x5 >> x.x6; }

    inline friend metastream& operator << (metastream& m, const STLMIX& x)
    {
        MSTRUCT_OPEN(m,"STLMIX")
        MM(m,"vector",x.x0)
        MM(m,"list",x.x1)
        MM(m,"deque",x.x2)
        MM(m,"set",x.x3)
        MM(m,"multiset",x.x4)
        MM(m,"map",x.x5)
        MM(m,"multimap",x.x6)
        MSTRUCT_CLOSE(m)
    }
};


void test()
{
    {
    token t = "fashion";
    std::ostringstream ost;
    ost << t;
    }

    //binstream out and in test

    bofstream bof("stl.test");

    const char* t = "some string";

    STLMIX x;
    x.x1.push_back("hallo");
    x.x1.push_back("co sa stallo");

    bof << t << x;
    bof.close();


    STLMIX y;
    bifstream bif("stl.test");

    bif >> t >> y;
    bif.close();

    ::free((void*)t);

    
    //metastream out and in test

    bof.open("stl_meta.test");
    fmtstreamxml2 txpo(bof);
    metastream meta(txpo);

    meta.stream_out(x);

    bof.close();


    bif.open("stl_meta.test");

    fmtstreamxml2 txpi(bif);
    meta.bind_formatting_stream(txpi);

    meta.stream_in(y);

    bif.close();
}

} //namespace coid
