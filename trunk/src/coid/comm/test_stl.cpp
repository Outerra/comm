
#include "coid/comm/binstream/stlstream.h"
#include "coid/comm/binstream/filestream.h"

namespace coid {

static void test()
{
    bofstream bof("test_stl");

    std::list<charstr> x1;
    std::deque<charstr> x2;

    bof << x1 << x2;
}

} //namespace coid
