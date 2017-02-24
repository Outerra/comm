
#include "../trait.h"
#include "../alloc/slotalloc.h"

namespace coid {

struct taskmaster
{

    template <typename Fn>
    void push(const Fn& fn) {
        _queue
    }


    slotalloc<uint64> _queue;
};

} //namespace coid

void test_job_queue()
{
    static const uint bits[] = {
        0b00000000000000000101110000100010,
        0b00000000000000000000000000000100,
    };

    uints i = coid::find_zero_bitrange(1, bits, bits + 2);
    DASSERT(i == 0);

    i = coid::find_zero_bitrange(3, bits, bits + 2);
    DASSERT(i == 2);

    i = coid::find_zero_bitrange(4, bits, bits + 2);
    DASSERT(i == 6);

    i = coid::find_zero_bitrange(29, bits, bits + 2);
    DASSERT(i == 35);

    i = coid::find_zero_bitrange(33, bits, bits + 2);
    DASSERT(i == 35);

}
