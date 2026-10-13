#include <comm/atomic/mpsc_queue.h>
#include <comm/commassert.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

/*
    Tests of coid::mpsc_queue: any number of producer threads, one consumer at a time.

    The concurrent test runs the producers and the consumer at the same time. Every item carries
    its producer and its sequence number, so the consumer can check both properties the queue
    promises: every item comes out exactly once, and the items of one producer come out in the
    order it pushed them.

    Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

namespace {

/// @brief An item that counts its live instances, for checking that the queue constructs and
///  destroys exactly what it holds
struct queue_counted_item
{
    int _value = 0;

    static inline std::atomic<int> alive = 0;

    explicit queue_counted_item(int value) : _value(value) { alive.fetch_add(1, std::memory_order_relaxed); }
    queue_counted_item(const queue_counted_item& other) : _value(other._value) { alive.fetch_add(1, std::memory_order_relaxed); }
    queue_counted_item(queue_counted_item&& other) noexcept : _value(other._value) { other._value = -1; alive.fetch_add(1, std::memory_order_relaxed); }
    queue_counted_item& operator=(const queue_counted_item& other) { _value = other._value; return *this; }
    queue_counted_item& operator=(queue_counted_item&& other) noexcept { _value = other._value; other._value = -1; return *this; }
    ~queue_counted_item() { alive.fetch_sub(1, std::memory_order_relaxed); }
};

/// @brief An item whose move assignment throws on request, for checking that a pop that fails
///  half way still destroys the item it took out of its node and keeps the container usable
struct throwing_queue_item
{
    int _value = 0;

    static inline std::atomic<int> alive = 0;
    static inline bool throw_on_move_assignment = false;

    explicit throwing_queue_item(int value) : _value(value) { alive.fetch_add(1, std::memory_order_relaxed); }
    throwing_queue_item(const throwing_queue_item& other) : _value(other._value) { alive.fetch_add(1, std::memory_order_relaxed); }
    throwing_queue_item(throwing_queue_item&& other) : _value(other._value) { alive.fetch_add(1, std::memory_order_relaxed); }
    throwing_queue_item& operator=(const throwing_queue_item& other) { _value = other._value; return *this; }
    throwing_queue_item& operator=(throwing_queue_item&& other) {
        if (throw_on_move_assignment) {
            throw 1;
        }
        _value = other._value;
        return *this;
    }
    ~throwing_queue_item() { alive.fetch_sub(1, std::memory_order_relaxed); }
};

/// @brief An item of the concurrent test: who pushed it, and its place in that producer's sequence
struct tagged_item
{
    unsigned _producer = 0;
    unsigned _sequence = 0;
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
void mpsc_queue_tests()
{
    // empty queue: pop and pop_if report false and leave the target alone
    {
        coid::mpsc_queue<int> q;
        int v = 7;
        RASSERT(!q.pop(v));
        RASSERT(!q.pop_if([](const int&) { return true; }, v));
        RASSERT(v == 7);
    }

    // first in, first out
    {
        coid::mpsc_queue<int> q;
        for (int i = 0; i < 5; ++i) {
            q.push(i);
        }
        for (int i = 0; i < 5; ++i) {
            int v = -1;
            RASSERT(q.pop(v) && v == i);
        }
        int v = -1;
        RASSERT(!q.pop(v));
    }

    // pushes after the consumer took the pushed items go behind them
    {
        coid::mpsc_queue<int> q;
        q.push(0);
        q.push(1);
        q.push(2);
        int v = -1;
        RASSERT(q.pop(v) && v == 0);        // takes 0, 1, 2 into the consumer's list
        q.push(3);
        q.push(4);
        for (int i = 1; i < 5; ++i) {
            RASSERT(q.pop(v) && v == i);
        }
        RASSERT(!q.pop(v));

        // and many rounds of it, with the nodes recycled through the node pool
        int expected = 0, next = 0;
        for (int round = 0; round < 1000; ++round) {
            q.push(next++);
            q.push(next++);
            RASSERT(q.pop(v) && v == expected++);
        }
        while (q.pop(v)) {
            RASSERT(v == expected++);
        }
        RASSERT(expected == next);
    }

    // pop_if removes the oldest item only when the condition holds; otherwise it stays in front
    {
        coid::mpsc_queue<int> q;
        q.push(10);
        q.push(20);
        int v = -1;
        RASSERT(!q.pop_if([](const int& item) { return item > 10; }, v));
        RASSERT(v == -1);
        RASSERT(q.pop_if([](const int& item) { return item == 10; }, v) && v == 10);
        RASSERT(q.pop_if([](const int& item) { return item > 10; }, v) && v == 20);
        RASSERT(!q.pop_if([](const int&) { return true; }, v));

        // the condition sees the oldest item, also when it was pushed after the last take
        q.push(1);
        RASSERT(q.pop(v) && v == 1);
        q.push(2);
        q.push(3);
        int seen = -1;
        RASSERT(!q.pop_if([&seen](const int& item) { seen = item; return false; }, v));
        RASSERT(seen == 2);
        RASSERT(q.pop(v) && v == 2);
        RASSERT(q.pop(v) && v == 3);
    }

    // push copies or moves the item in, a pop moves it out and destroys the one in the node
    {
        coid::mpsc_queue<queue_counted_item> q;
        queue_counted_item original(1);
        q.push(original);                           // a copy
        RASSERT(original._value == 1);
        RASSERT(queue_counted_item::alive == 2);
        q.push(queue_counted_item(2));              // moved in, the temporary is gone
        RASSERT(queue_counted_item::alive == 3);

        queue_counted_item out(0);
        RASSERT(q.pop(out) && out._value == 1);
        RASSERT(queue_counted_item::alive == 3);    // original, out, the one left in the queue
        RASSERT(q.pop(out) && out._value == 2);
        RASSERT(queue_counted_item::alive == 2);    // the queue holds nothing any more
    }
    RASSERT(queue_counted_item::alive == 0);

    // the destructor destroys the items left, both the pushed ones and the ones already taken
    {
        {
            coid::mpsc_queue<queue_counted_item> q;
            q.push(queue_counted_item(0));
            q.push(queue_counted_item(1));
            q.push(queue_counted_item(2));
            queue_counted_item out(0);
            RASSERT(q.pop(out));                    // 1 and 2 are in the consumer's list now
            q.push(queue_counted_item(3));          // and 3 in the shared one
            RASSERT(queue_counted_item::alive == 4);
        }
        RASSERT(queue_counted_item::alive == 0);
    }

    // move-only items
    {
        coid::mpsc_queue<std::unique_ptr<int>> q;
        q.push(std::make_unique<int>(5));
        q.push(std::make_unique<int>(6));
        std::unique_ptr<int> out;
        RASSERT(q.pop(out) && out && *out == 5);
        RASSERT(q.pop_if([](const std::unique_ptr<int>& item) { return *item == 6; }, out) && *out == 6);
        RASSERT(!q.pop(out) && out && *out == 6);
    }

    // a move out that throws: the item taken out of its node is destroyed all the same, the node
    // goes back to the pool and the queue stays usable
    {
        coid::mpsc_queue<throwing_queue_item> q;
        q.push(throwing_queue_item(1));
        q.push(throwing_queue_item(2));
        throwing_queue_item out(0);
        RASSERT(throwing_queue_item::alive == 3);

        bool thrown = false;
        throwing_queue_item::throw_on_move_assignment = true;
        try {
            q.pop(out);
        }
        catch (int) {
            thrown = true;
        }
        throwing_queue_item::throw_on_move_assignment = false;

        RASSERT(thrown);
        RASSERT(out._value == 0);                   // nothing reached the target
        RASSERT(throwing_queue_item::alive == 2);   // the item of the failed pop is gone, 2 is left

        RASSERT(q.pop(out) && out._value == 2);
        RASSERT(!q.pop(out));
        RASSERT(throwing_queue_item::alive == 1);   // only out
    }
    RASSERT(throwing_queue_item::alive == 0);

    // producers and the consumer at the same time: every item once, each producer's in its order
    {
        constexpr unsigned producer_count = 6;
        constexpr unsigned per_producer = 100000;

        coid::mpsc_queue<tagged_item> q;
        std::atomic<bool> go = false;
        std::vector<std::thread> producers;
        for (unsigned p = 0; p < producer_count; ++p) {
            producers.emplace_back([&q, &go, p] {
                while (!go.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                for (unsigned n = 0; n < per_producer; ++n) {
                    q.push(tagged_item{ p, n });
                }
            });
        }

        std::vector<unsigned> next_expected(producer_count, 0);
        unsigned received = 0, out_of_order = 0, unknown = 0;
        go.store(true, std::memory_order_release);

        while (received < producer_count * per_producer) {
            tagged_item item;
            if (!q.pop(item)) {
                std::this_thread::yield();
                continue;
            }
            ++received;
            if (item._producer >= producer_count) {
                ++unknown;
                continue;
            }
            if (item._sequence != next_expected[item._producer]) {
                ++out_of_order;
            }
            next_expected[item._producer] = item._sequence + 1;
        }

        for (std::thread& t : producers) {
            t.join();
        }

        tagged_item extra;
        RASSERT(!q.pop(extra));                     // nothing duplicated
        RASSERT(unknown == 0);
        RASSERT(out_of_order == 0);
        for (unsigned p = 0; p < producer_count; ++p) {
            RASSERT(next_expected[p] == per_producer);
        }
    }
}
