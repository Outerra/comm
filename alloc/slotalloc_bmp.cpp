#include "slotalloc_bmp.h"
#include "../str.h"
#include "../rnd.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

struct test_data
{
    coid::charstr _name;
    int _a;

    test_data(const coid::token &name, const int a)
        : _name(name)
        , _a(a)
    {}
};

void coid::test::slotalloc_bmp()
{
    coid::slotalloc_bmp<test_data> data;
    coid::dynarray<uint> handles;
    coid::rnd_strong rnd(24324);

    uint counter = 128;
    while (counter--) {
        for (int i = 0; i != 2048; ++i) {
            const test_data * const d =
                new (data.add_uninit()) test_data("Hello world!", 342);
            const uint handle = data.get_item_id(d);

            if (!data.is_valid(handle)) {
                DASSERT(false && "this should not happen!");
                data.is_valid(handle);
            }

            handles.push(handle);
        }

        auto i = handles.ptr();
        auto e = handles.ptre();
        uint hn = handles.size() / 2;

        while (hn--) {
            const uint index = rnd.rand() % handles.size();
            const uint handle = handles[index];
            handles.del(index);
            data.del(handle);
        }
    }

    uint * i = handles.ptr();
    uint * const e = handles.ptre();
    while (i != e) {
        if (!data.is_valid(*i)) {
            DASSERT(false && "this should not happen!");
            data.is_valid(*i);
        }
        ++i;
    }

    test_data * d = new (data.add_uninit()) test_data("Hello world!", 342);
    uint item = data.get_item_id(d);

    item = data.first();
    while (item != -1) {
        if (item == 123)
            int a = 0;
        if (!data.is_valid(item)) {
            DASSERT(false && "this should not happen!");
            data.is_valid(item);
        }

        printf("%u %s\n", item, data.get_item(item)->_name);
        item = data.next(item);
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
