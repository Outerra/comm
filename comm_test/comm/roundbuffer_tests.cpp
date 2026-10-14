#include <comm/roundbuffer.h>
#include <comm/commassert.h>

#include <atomic>
#include <thread>

/*
    Tests of coid::roundbuffer: a ring buffer for one writer thread and one reader thread.

    The concurrent test runs the writer and the reader at the same time, and the reader checks
    that it gets every value once and in the order it was written. The buffer is small, so the
    writer keeps running into a full buffer and has to try again - which is when a slot the
    reader has given back gets written again.

    Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

////////////////////////////////////////////////////////////////////////////////
void roundbuffer_tests()
{
    // empty buffer: pop reports false and leaves the target alone
    {
        coid::roundbuffer<int, 8> rb;
        int v = 7;
        RASSERT(!rb.pop(v));
        RASSERT(v == 7);
    }

    // first in, first out, also across the wrap of the indices
    {
        coid::roundbuffer<int, 8> rb;
        int next_push = 0, next_pop = 0;
        for (int round = 0; round < 100; ++round) {
            for (int k = 0; k < 5; ++k) {
                RASSERT(rb.push(next_push++));
            }
            for (int k = 0; k < 5; ++k) {
                int v = -1;
                RASSERT(rb.pop(v) && v == next_pop++);
            }
            int v = -1;
            RASSERT(!rb.pop(v));
        }
    }

    // a buffer of SIZE slots holds SIZE - 1 items; push on a full buffer reports false and changes
    // nothing, and after a pop there is room again
    {
        coid::roundbuffer<int, 8> rb;
        for (int i = 0; i < 7; ++i) {
            RASSERT(rb.push(i));
        }
        RASSERT(!rb.push(100));
        int v = -1;
        RASSERT(rb.pop(v) && v == 0);
        RASSERT(rb.push(7));
        RASSERT(!rb.push(101));
        for (int i = 1; i < 8; ++i) {
            RASSERT(rb.pop(v) && v == i);
        }
        RASSERT(!rb.pop(v));
    }

    // one writer and one reader at the same time: every value once, in the order written
    {
        constexpr unsigned count = 1000000;

        coid::roundbuffer<unsigned, 8> rb;

        std::thread writer([&rb] {
            for (unsigned n = 0; n < count; ++n) {
                while (!rb.push(n)) {               // full: the reader is behind
                    std::this_thread::yield();
                }
            }
        });

        unsigned expected = 0, out_of_order = 0;
        while (expected < count) {
            unsigned v;
            if (!rb.pop(v)) {
                std::this_thread::yield();
                continue;
            }
            if (v != expected) {
                ++out_of_order;
            }
            ++expected;
        }
        writer.join();

        unsigned extra;
        RASSERT(!rb.pop(extra));
        RASSERT(out_of_order == 0);
    }
}
