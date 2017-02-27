
#include "../trait.h"
#include "../alloc/slotalloc.h"
#include "../bitrange.h"
#include "../log/logger.h"

namespace coid {

class taskmaster
{
public:

    template <typename Fn, typename ...Args>
    void push( const Fn& fn, Args&& ...args ) {
        using callfn = invoker<Fn, Args...>;

        void* p = _queue.add_range_uninit(align_to_chunks(sizeof(callfn), sizeof(uint64)));

        new(p) callfn(fn, std::forward<Args>(args)...);
    }

    void invoke() {
        reinterpret_cast<invoker_base*>(_queue.get_item(0))->invoke();
    }

protected:

    struct invoker_base {
        virtual void invoke() = 0;
    };

    template <typename Fn, typename ...Args>
    struct invoker : invoker_base
    {
        invoker(const Fn& fn, Args&& ...args)
            : _fn(fn)
            , _tuple(std::forward<Args>(args)...)
        {}

        void invoke() override final {
            invoke_impl(make_index_sequence<sizeof...(Args)>());
        }

    private:

        template <size_t ...I>
        void invoke_impl(index_sequence<I...>) {
            _fn(std::get<I>(_tuple)...);
        }

        typedef std::tuple<Args...> tuple_t;

        Fn _fn;
        tuple_t _tuple;
    };

private:

    slotalloc<uint64> _queue;
};

} //namespace coid

void test_job_queue()
{
#if 0
    static uint bits[] = {
#elif 1
    volatile uint bits[] = {
#elif 0
    std::atomic_uint bits[] = {
#endif
        0x00005c22, //0b00000000000000000101110000100010,
        0x00000004, //0b00000000000000000000000000000100,
    };

    uints i = coid::find_zero_bitrange(1, bits, bits + 2);
    DASSERT(i == 0);

    i = coid::find_zero_bitrange(2, bits, bits + 2);
    DASSERT(i == 2);

    i = coid::find_zero_bitrange(3, bits, bits + 2);
    DASSERT(i == 2);

    i = coid::find_zero_bitrange(4, bits, bits + 2);
    DASSERT(i == 6);

    i = coid::find_zero_bitrange(29, bits, bits + 2);
    DASSERT(i == 35);

    i = coid::find_zero_bitrange(33, bits, bits + 2);
    DASSERT(i == 35);

    coid::set_bitrange(28, 6, bits);
    DASSERT( bits[0] == 0xf0005c22 //0b11110000000000000101110000100010
        &&   bits[1] == 0x00000007 //0b00000000000000000000000000000111
    );

    coid::set_bitrange(59, 5, bits);
    DASSERT( bits[1] == 0xf8000007 ); //0b11111000000000000000000000000111 );

    coid::clear_bitrange(16, 48, bits);
    DASSERT( bits[0] == 0x00005c22 //0b00000000000000000101110000100010
        &&   bits[1] == 0 );



    coid::taskmaster task;

    auto job1 = [](int a, void* b) {
        coidlog_info("jobtest", "a: " << a << ", b: " << (uints)b);
    };

    task.push(job1, 1, nullptr);

    task.invoke();
}
