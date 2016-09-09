
#include "../str.h"
#include "../radix.h"
#include "../hash/slothash.h"
#include "../alloc/slotring.h"

namespace coid {
void test();
void metastream_test();
}

void metastream_test3();
void metastream_test2();
int main_atomic(int argc, char * argv[]);

void regex_test();
void test_malloc();

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

using namespace coid;

struct value {
    charstr key;

    operator token() const { return key; }
};

////////////////////////////////////////////////////////////////////////////////
int main( int argc, char* argv[] )
{
#if 0
    static_assert( std::is_trivially_move_constructible<dynarray<char>>::value, "non-trivial move");
    static_assert( std::is_trivially_move_constructible<charstr>::value, "non-trivial move");
    static_assert( std::is_trivially_move_constructible<dynarray<int>>::value, "non-trivial move");
    static_assert( std::is_trivially_move_constructible<dynarray<charstr>>::value, "non-trivial move");
    static_assert( std::is_trivially_move_constructible<dynarray<dynarray<int>>>::value, "non-trivial move");
#endif

#if _MSC_VER >= 1800
    slothash<value, token> hash;
    bool isnew;
    hash.find_or_insert_value_slot("foo", &isnew);
    hash.find_or_insert_value_slot("foo", &isnew);
    hash.find_or_insert_value_slot("bar", &isnew);


    slotring<value> ring(8);

#endif

    uint64 stuff[] = {7000, 45, 2324, 11, 0, 222};
    radixi<uint64, uint, uint64> rx;
    const uint* idx = rx.sort(true, stuff, sizeof(stuff)/sizeof(stuff[0]));

    //coid::test();
    //test_malloc();

    //metastream_test3();
    //metastream_test2();
	//float_test();

    //main_atomic(argc, argv);
    //coid::test();
    metastream_test();
    regex_test();
    return 0;
}