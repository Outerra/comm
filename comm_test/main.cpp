
#include "../str.h"

namespace coid {
void test();
void metastream_test();
}

void metastream_test2();
int main_atomic(int argc, char * argv[]);

void regex_test();

void float_test()
{
    char buf[32];
    double v = 12345678.90123456789;
    int n = 8;
    int nfrac = 3;

    while(1)
    {
        ::memset(buf, 0, 32);
        coid::charstrconv::append_float(buf, buf+n, v, nfrac);
    }
}

////////////////////////////////////////////////////////////////////////////////
int main( int argc, char* argv[] )
{
    metastream_test2();
	//float_test();

    //main_atomic(argc, argv);
    //coid::test();
    coid::metastream_test();
    regex_test();
    return 0;
}