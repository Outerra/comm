#include <comm/alloc/memtrack.h>
#include <comm/commassert.h>

#include <thread>
#include <vector>

/*
    Tests of the memory tracker (comm/alloc/memtrack.h), which counts the allocations of every type
    declared with COIDNEWDELETE. It exists in debug builds only (MEMTRACK_ENABLED), so in release
    builds memtrack_tests() does nothing.

    Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

namespace {

#ifdef MEMTRACK_ENABLED

struct memtrack_probe
{
    COIDNEWDELETE(memtrack_probe);

    int _value = 0;
};

/// @brief Finds the tracker's entry for memtrack_probe
/// @return true if the type has an entry
bool find_probe_entry(coid::memtrack& entry)
{
    std::vector<coid::memtrack> entries(coid::memtrack_count() + 64);
    const unsigned count = coid::memtrack_list(entries.data(), unsigned(entries.size()), false);
    const unsigned hash = coid::type_info::get<memtrack_probe>().hash;

    for (unsigned i = 0; i < count; ++i) {
        if (entries[i].hash == hash) {
            entry = entries[i];
            return true;
        }
    }
    return false;
}

#endif

} // namespace

////////////////////////////////////////////////////////////////////////////////
void memtrack_tests()
{
#ifdef MEMTRACK_ENABLED
    const bool was_enabled = coid::memtrack_enable(true);

    // allocations and frees on several threads at once are all counted - none is mistaken for the
    // tracker's own allocation on another thread
    {
        coid::memtrack before;
        const bool had_entry = find_probe_entry(before);
        const unsigned current_before = had_entry ? before.ncurallocs : 0;
        const unsigned lifetime_before = had_entry ? before.nlifeallocs : 0;

        constexpr unsigned thread_count = 8;
        constexpr unsigned per_thread = 20000;

        std::vector<std::thread> threads;
        for (unsigned t = 0; t < thread_count; ++t) {
            threads.emplace_back([] {
                for (unsigned n = 0; n < per_thread; ++n) {
                    memtrack_probe* p = new memtrack_probe;
                    p->_value = int(n);
                    delete p;
                }
            });
        }
        for (std::thread& t : threads) {
            t.join();
        }

        coid::memtrack after;
        RASSERT(find_probe_entry(after));
        RASSERT(after.ncurallocs == current_before);
        RASSERT(after.nlifeallocs == lifetime_before + thread_count * per_thread);
    }

    coid::memtrack_enable(was_enabled);
#endif
}
