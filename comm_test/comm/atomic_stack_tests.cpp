#include <comm/atomic/stack.h>
#include <comm/atomic/pool.h>
#include <comm/list.h>
#include <comm/commassert.h>

#include <atomic>
#include <memory>
#include <type_traits>
#include <thread>
#include <vector>

/*
    Tests of the lock-free stacks built on atomic::detail::stack_base: coid::stack (holds values,
    its nodes come from a global pool of nodes) and coid::pool (two stacks: idle objects and free
    nodes), plus coid::list, which takes its nodes from a coid::pool.

    The concurrent tests recycle a fixed set of items: every thread takes an item and puts it back,
    many times over. This is the pattern of the pools, and the one in which an ABA problem shows -
    a head that returns to the same node with a different next pointer. Each item counts how often
    it comes out of the final drain, so a lost item (count 0), a duplicated one (count 2) or a cycle
    in the list (the drain runs past the item count) fails the test.

    Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

namespace {

constexpr int concurrent_item_count = 64;
constexpr int concurrent_iterations = 200000;

unsigned concurrent_thread_count()
{
    const unsigned hw = std::thread::hardware_concurrency();
    return hw < 4 ? 4 : (hw > 8 ? 8 : hw);
}

/// @brief Runs fn(thread_index) on the given number of threads, all released at once
template<typename Fn>
void run_threads(unsigned thread_count, Fn fn)
{
    std::atomic<bool> go = false;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (unsigned i = 0; i < thread_count; ++i) {
        threads.emplace_back([&go, &fn, i] {
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            fn(i);
        });
    }

    go.store(true, std::memory_order_release);

    for (std::thread& t : threads) {
        t.join();
    }
}

/// @brief Takes everything out and counts how often each item came out; stops after max_takes
/// @param take - returns the next item, nullptr when empty
/// @return number of items taken
template<typename Item, typename TakeFn, typename IdFn>
int drain_and_count(TakeFn take, int* counts, int item_count, int max_takes, IdFn id_of)
{
    int taken = 0;
    while (taken < max_takes) {
        Item* item = take();
        if (!item) {
            break;
        }
        const int id = id_of(item);
        RASSERT(id >= 0 && id < item_count);
        if (id >= 0 && id < item_count) {
            ++counts[id];
        }
        ++taken;
    }
    return taken;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief An item whose move assignment throws on request, for checking that a pop that fails
///  half way still destroys the item it took out of its node and keeps the container usable
struct throwing_stack_item
{
    int _value = 0;

    static inline std::atomic<int> alive = 0;
    static inline bool throw_on_move_assignment = false;

    explicit throwing_stack_item(int value) : _value(value) { alive.fetch_add(1, std::memory_order_relaxed); }
    throwing_stack_item(const throwing_stack_item& other) : _value(other._value) { alive.fetch_add(1, std::memory_order_relaxed); }
    throwing_stack_item(throwing_stack_item&& other) : _value(other._value) { alive.fetch_add(1, std::memory_order_relaxed); }
    throwing_stack_item& operator=(const throwing_stack_item& other) { _value = other._value; return *this; }
    throwing_stack_item& operator=(throwing_stack_item&& other) {
        if (throw_on_move_assignment) {
            throw 1;
        }
        _value = other._value;
        return *this;
    }
    ~throwing_stack_item() { alive.fetch_sub(1, std::memory_order_relaxed); }
};

/// @brief An item that counts its live instances, for checking that the stack constructs and
///  destroys exactly what it holds
struct counted_item
{
    int _value = 0;

    static inline std::atomic<int> alive = 0;

    explicit counted_item(int value) : _value(value) { alive.fetch_add(1, std::memory_order_relaxed); }
    counted_item(const counted_item& other) : _value(other._value) { alive.fetch_add(1, std::memory_order_relaxed); }
    counted_item(counted_item&& other) noexcept : _value(other._value) { other._value = -1; alive.fetch_add(1, std::memory_order_relaxed); }
    counted_item& operator=(const counted_item& other) { _value = other._value; return *this; }
    counted_item& operator=(counted_item&& other) noexcept { _value = other._value; other._value = -1; return *this; }
    ~counted_item() { alive.fetch_sub(1, std::memory_order_relaxed); }
};

void stack_tests()
{
    // empty stack: pop reports false and leaves the target alone
    {
        coid::stack<int> s;
        int v = 7;
        RASSERT(!s.pop(v));
        RASSERT(v == 7);
    }

    // last in, first out
    {
        coid::stack<int> s;
        for (int i = 0; i < 5; ++i) {
            s.push(i);
        }
        for (int i = 4; i >= 0; --i) {
            int v = -1;
            RASSERT(s.pop(v) && v == i);
        }
        int v = -1;
        RASSERT(!s.pop(v));
    }

    // the same value may be pushed more than once - each push is an item of its own
    {
        coid::stack<int> s;
        s.push(7);
        s.push(7);
        int a = 0, b = 0, c = 0;
        RASSERT(s.pop(a) && a == 7);
        RASSERT(s.pop(b) && b == 7);
        RASSERT(!s.pop(c));
    }

    // interleaved pushes and pops, with the nodes recycled through the node pool
    {
        coid::stack<int> s;
        for (int round = 0; round < 1000; ++round) {
            s.push(0);
            s.push(1);
            int v = -1;
            RASSERT(s.pop(v) && v == 1);
            s.push(2);
            RASSERT(s.pop(v) && v == 2);
            RASSERT(s.pop(v) && v == 0);
            RASSERT(!s.pop(v));
        }
    }

    // pointers are values too - the stack never owns or deletes what they point to
    {
        int items[3] = { 0, 1, 2 };
        coid::stack<int*> s;
        for (int& i : items) {
            s.push(&i);
        }
        int* p = nullptr;
        RASSERT(s.pop(p) && p == &items[2]);
        RASSERT(s.pop(p) && p == &items[1]);
        RASSERT(s.pop(p) && p == &items[0]);
        RASSERT(!s.pop(p));
    }

    // push copies or moves the item in, pop moves it out and destroys the one in the node
    {
        coid::stack<counted_item> s;
        counted_item original(1);
        s.push(original);                           // a copy
        RASSERT(original._value == 1);
        RASSERT(counted_item::alive == 2);
        s.push(counted_item(2));                    // moved in, the temporary is gone
        RASSERT(counted_item::alive == 3);

        counted_item out(0);
        RASSERT(s.pop(out) && out._value == 2);
        RASSERT(counted_item::alive == 3);          // original, out, the one left in the stack
        RASSERT(s.pop(out) && out._value == 1);
        RASSERT(counted_item::alive == 2);          // the stack holds nothing any more
    }
    RASSERT(counted_item::alive == 0);

    // the destructor destroys the items left in the stack
    {
        {
            coid::stack<counted_item> s;
            for (int i = 0; i < 4; ++i) {
                s.push(counted_item(i));
            }
            RASSERT(counted_item::alive == 4);
        }
        RASSERT(counted_item::alive == 0);
    }

    // move-only items, and a type with no default constructor - the nodes hold raw storage
    {
        coid::stack<std::unique_ptr<int>> s;
        s.push(std::make_unique<int>(5));
        std::unique_ptr<int> out;
        RASSERT(s.pop(out) && out && *out == 5);
        RASSERT(!s.pop(out) && out && *out == 5);   // an empty pop leaves the target alone

        static_assert(!std::is_default_constructible_v<counted_item>);
        coid::stack<counted_item> c;
        c.push(counted_item(9));
        counted_item target(0);
        RASSERT(c.pop(target) && target._value == 9);
    }
    RASSERT(counted_item::alive == 0);

    // a move out that throws: the item taken out of its node is destroyed all the same, the node
    // goes back to the pool and the stack stays usable
    {
        coid::stack<throwing_stack_item> s;
        s.push(throwing_stack_item(1));
        s.push(throwing_stack_item(2));
        throwing_stack_item out(0);
        RASSERT(throwing_stack_item::alive == 3);

        bool thrown = false;
        throwing_stack_item::throw_on_move_assignment = true;
        try {
            s.pop(out);
        }
        catch (int) {
            thrown = true;
        }
        throwing_stack_item::throw_on_move_assignment = false;

        RASSERT(thrown);
        RASSERT(out._value == 0);                   // nothing reached the target
        RASSERT(throwing_stack_item::alive == 2);   // the item of the failed pop is gone, 1 is left

        RASSERT(s.pop(out) && out._value == 1);
        RASSERT(!s.pop(out));
        RASSERT(throwing_stack_item::alive == 1);   // only out
    }
    RASSERT(throwing_stack_item::alive == 0);

    // concurrent recycling: every value comes out of the final drain exactly once
    {
        coid::stack<int> s;
        for (int i = 0; i < concurrent_item_count; ++i) {
            s.push(i);
        }

        run_threads(concurrent_thread_count(), [&s](unsigned) {
            for (int n = 0; n < concurrent_iterations; ++n) {
                int v;
                if (s.pop(v)) {
                    s.push(v);
                }
            }
        });

        int counts[concurrent_item_count] = {};
        int taken = 0;
        int v = -1;
        while (taken <= concurrent_item_count && s.pop(v)) {
            RASSERT(v >= 0 && v < concurrent_item_count);
            if (v >= 0 && v < concurrent_item_count) {
                ++counts[v];
            }
            ++taken;
        }

        RASSERT(taken == concurrent_item_count);
        for (int i = 0; i < concurrent_item_count; ++i) {
            RASSERT(counts[i] == 1);
        }
    }

    // concurrent pushes from several threads, then a drain: every value exactly once
    {
        const unsigned thread_count = concurrent_thread_count();
        constexpr int per_thread = 10000;
        const int total = int(thread_count) * per_thread;

        coid::stack<int> s;
        run_threads(thread_count, [&s](unsigned t) {
            for (int n = 0; n < per_thread; ++n) {
                s.push(int(t) * per_thread + n);
            }
        });

        std::vector<int> counts(total, 0);
        int taken = 0;
        int v = -1;
        while (taken <= total && s.pop(v)) {
            RASSERT(v >= 0 && v < total);
            if (v >= 0 && v < total) {
                ++counts[v];
            }
            ++taken;
        }

        RASSERT(taken == total);
        for (int c : counts) {
            RASSERT(c == 1);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
struct pooled_object
{
    int _value = 0;

    static inline std::atomic<int> alive = 0;

    pooled_object() { alive.fetch_add(1, std::memory_order_relaxed); }
    ~pooled_object() { alive.fetch_sub(1, std::memory_order_relaxed); }
};

void pool_tests()
{
    // get_item on an empty pool returns nothing, get_or_create_item makes a new object
    {
        coid::pool<pooled_object> p;
        RASSERT(p.get_item() == nullptr);

        pooled_object* a = p.get_or_create_item();
        RASSERT(a != nullptr);
        RASSERT(pooled_object::alive == 1);

        // released objects come back last in, first out, without being reconstructed
        a->_value = 42;
        pooled_object* released = a;
        p.release_item(released);
        RASSERT(released == nullptr);
        RASSERT(p.get_or_create_item() == a);
        RASSERT(a->_value == 42);
        RASSERT(pooled_object::alive == 1);

        // reset deletes the pooled objects
        p.release_item(a);
        pooled_object* b = p.get_or_create_item();  // the same object again
        pooled_object* c = p.get_or_create_item();  // a new one
        RASSERT(pooled_object::alive == 2);
        p.release_item(b);
        p.release_item(c);
        p.reset();
        RASSERT(pooled_object::alive == 0);
        RASSERT(p.get_item() == nullptr);
    }

    // the pool takes objects it did not create, and hands them out like its own
    {
        coid::pool<pooled_object> p;
        pooled_object* external = new pooled_object;
        pooled_object* released = external;
        p.release_item(released);
        RASSERT(p.get_item() == external);
        RASSERT(p.get_item() == nullptr);
        delete external;
    }
    RASSERT(pooled_object::alive == 0);

    // destroyed with objects in it - the pool deletes them, but not the ones taken out of it
    {
        pooled_object* taken = nullptr;
        {
            coid::pool<pooled_object> p;
            pooled_object* objects[3] = { new pooled_object, new pooled_object, new pooled_object };
            for (pooled_object*& o : objects) {
                p.release_item(o);
            }
            taken = p.get_item();
            RASSERT(pooled_object::alive == 3);
        }
        RASSERT(pooled_object::alive == 1);
        delete taken;
    }
    RASSERT(pooled_object::alive == 0);

    // concurrent recycling: every object comes out of the final drain exactly once
    {
        coid::pool<pooled_object> p;
        for (int i = 0; i < concurrent_item_count; ++i) {
            pooled_object* o = new pooled_object;
            o->_value = i;
            p.release_item(o);
        }

        run_threads(concurrent_thread_count(), [&p](unsigned) {
            for (int n = 0; n < concurrent_iterations; ++n) {
                if (pooled_object* o = p.get_item()) {
                    p.release_item(o);
                }
            }
        });

        int counts[concurrent_item_count] = {};
        std::vector<pooled_object*> drained;
        const int taken = drain_and_count<pooled_object>([&p, &drained] {
                pooled_object* o = p.get_item();
                if (o) {
                    drained.push_back(o);
                }
                return o;
            }, counts, concurrent_item_count, concurrent_item_count + 1, [](pooled_object* o) { return o->_value; });
        for (pooled_object* o : drained) {
            p.release_item(o);
        }

        RASSERT(taken == concurrent_item_count);
        for (int i = 0; i < concurrent_item_count; ++i) {
            RASSERT(counts[i] == 1);
        }
        p.reset();
    }
    RASSERT(pooled_object::alive == 0);

    // concurrent create and release: no object is handed out twice at the same time
    {
        coid::pool<pooled_object> p;
        std::atomic<int> errors = 0;

        run_threads(concurrent_thread_count(), [&p, &errors](unsigned t) {
            for (int n = 0; n < concurrent_iterations / 4; ++n) {
                pooled_object* o = p.get_or_create_item();
                // a stamp only this thread writes while it holds the object
                o->_value = int(t) + 1;
                for (int k = 0; k < 4; ++k) {
                    if (o->_value != int(t) + 1) {
                        errors.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                o->_value = 0;
                p.release_item(o);
            }
        });

        RASSERT(errors == 0);
        RASSERT(pooled_object::alive <= int(concurrent_thread_count()));
        p.reset();
        RASSERT(pooled_object::alive == 0);
    }
}

////////////////////////////////////////////////////////////////////////////////
struct list_item
{
    int _value = 0;

    static inline int alive = 0;

    list_item(int value) : _value(value) { ++alive; }
    list_item(const list_item& other) : _value(other._value) { ++alive; }
    ~list_item() { --alive; }
};

void list_tests()
{
    // order, erase, and nodes reused through the node pool
    {
        coid::list<int> l;
        RASSERT(l.is_empty());

        for (int i = 1; i <= 5; ++i) {
            l.push_back(i);
        }
        l.push_front(0);

        int expected = 0;
        for (int v : l) {
            RASSERT(v == expected);
            ++expected;
        }
        RASSERT(expected == 6);

        RASSERT(l.erase(3));
        RASSERT(!l.erase(3));

        int front = -1, back = -1;
        RASSERT(l.pop_front(front) && front == 0);
        RASSERT(l.pop_back(back) && back == 5);

        // the nodes of the removed items go to the node pool and come back from it
        for (int round = 0; round < 1000; ++round) {
            l.push_back(round);
            int v = -1;
            RASSERT(l.pop_back(v) && v == round);
        }

        const int remaining[] = { 1, 2, 4 };
        int k = 0;
        for (int v : l) {
            RASSERT(k < 3 && v == remaining[k]);
            ++k;
        }
        RASSERT(k == 3);

        l.clear();
        RASSERT(l.is_empty());
    }

    // items are constructed into the nodes and destroyed when removed
    {
        coid::list<list_item> l;
        l.push_back(list_item(1));
        l.push_back(list_item(2));
        RASSERT(list_item::alive == 2);

        l.clear();
        RASSERT(list_item::alive == 0);
        RASSERT(l.is_empty());
    }

    // the destructor destroys the items left in the list
    {
        {
            coid::list<list_item> l;
            l.push_back(list_item(1));
            l.push_back(list_item(2));
            l.push_back(list_item(3));
            RASSERT(list_item::alive == 3);
        }
        RASSERT(list_item::alive == 0);
    }
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
void atomic_stack_tests()
{
    stack_tests();
    pool_tests();
    list_tests();
}
