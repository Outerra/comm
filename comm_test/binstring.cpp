
#include <comm/binstring.h>
#include <limits>

using namespace coid;

template <class T>
void varint_test(binstring& bs)
{
    bs.write_varint(T(0));
    RASSERT(bs.read_varint<T>() == 0);

    bs.write_varint(T(1));
    RASSERT(bs.read_varint<T>() == 1);

    bs.write_varint(T(127));
    RASSERT(bs.read_varint<T>() == 127);

    bs.write_varint(T(std::numeric_limits<T>::max()));
    RASSERT(bs.read_varint<T>() == std::numeric_limits<T>::max());

    if constexpr (!std::is_unsigned_v<T>) {
        bs.write_varint(T(-1));
        RASSERT(bs.read_varint<T>() == -1);

        bs.write_varint(T(std::numeric_limits<T>::min()));
        RASSERT(bs.read_varint<T>() == std::numeric_limits<T>::min());
    }
    else {
        bs.write_varint(T(255));
        RASSERT(bs.read_varint<T>() == 255);
    }
}

void binstring_test()
{
    binstring bs;

    varint_test<uint8>(bs);
    varint_test<uint16>(bs);
    varint_test<uint32>(bs);
    varint_test<uint64>(bs);

    varint_test<int8>(bs);
    varint_test<int16>(bs);
    varint_test<int32>(bs);
    varint_test<int64>(bs);
}
