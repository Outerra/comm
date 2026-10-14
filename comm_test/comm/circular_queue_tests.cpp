#include <comm/containers/circular_queue.h>
#include <comm/commassert.h>

#include <vector>

/*
    Tests of coid::circular_queue: a ring of the most recently pushed items, where a push into a
    full queue overwrites the oldest item.

    Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

namespace {

/// @brief The items of a queue in the given direction, as for_each visits them
template <typename Queue>
std::vector<int> items_of(const Queue& q, coid::iterator_direction dir = coid::iterator_direction::forward)
{
    std::vector<int> items;
    q.for_each([&items](int& v) { items.push_back(v); }, dir);
    return items;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
void circular_queue_tests()
{
    // empty queue
    {
        coid::circular_queue<int, 4> q;
        RASSERT(q.is_empty());
        RASSERT(q.size() == 0);
        RASSERT(items_of(q).empty());
    }

    // fewer items than it holds: forward from the oldest, backward from the newest
    {
        coid::circular_queue<int, 4> q;
        q.push(1);
        RASSERT(!q.is_empty());
        RASSERT(q.size() == 1);
        RASSERT(items_of(q) == std::vector<int>({ 1 }));

        q.push(2);
        q.push(3);
        RASSERT(q.size() == 3);
        RASSERT(items_of(q) == std::vector<int>({ 1, 2, 3 }));
        RASSERT(items_of(q, coid::iterator_direction::backward) == std::vector<int>({ 3, 2, 1 }));
    }

    // it holds at least min_size items, also when min_size is a power of two
    {
        coid::circular_queue<int, 4> q;
        for (int i = 1; i <= 4; ++i) {
            q.push(i);
        }
        RASSERT(q.size() == 4);
        RASSERT(items_of(q) == std::vector<int>({ 1, 2, 3, 4 }));
    }

    // a full queue: every further push overwrites the oldest item, the count stays the same
    {
        coid::circular_queue<int, 4> q;

        // fill it: the count grows with every push until a push has to overwrite the oldest item
        int pushed = 0;
        int held = 0;
        do {
            held = int(q.size());
            q.push(++pushed);
        } while (int(q.size()) > held);

        RASSERT(held >= 4);
        RASSERT(int(q.size()) == held);

        for (int round = 0; round < 3 * held; ++round) {
            q.push(++pushed);
            RASSERT(int(q.size()) == held);
        }

        // the last held values, oldest first
        std::vector<int> expected;
        for (int v = pushed - held + 1; v <= pushed; ++v) {
            expected.push_back(v);
        }
        RASSERT(items_of(q) == expected);

        std::vector<int> reversed(expected.rbegin(), expected.rend());
        RASSERT(items_of(q, coid::iterator_direction::backward) == reversed);
    }

    // push() hands out the slot of the new item
    {
        coid::circular_queue<int, 4> q;
        q.push() = 7;
        q.push() = 8;
        RASSERT(items_of(q) == std::vector<int>({ 7, 8 }));
    }
}
