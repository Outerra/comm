#include <comm/bitfield.h>

void test_bitfield_insert_bits()
{
    struct src_data
    {
        uint8 bytes[16] = { 0b1110'0000, 0b1111'1001, 0b1100'0000, 0,
                     0, 0, 0, 0b1110'0000,
                     0b1111'1001, 0, 0, 0,
                     0, 0, 0, 0
        };
    }src;


    coid::bitfield<100> field;

    // test read offset
    {
        field.insert_bits(src, 3, 0, 5);
        DASSERT(field.is_bit_set(0));
        DASSERT(field.is_bit_set(1));
        DASSERT(field.is_bit_set(2));
    }

    // test write offset
    {
        field.insert_bits(src, 3, 3, 5);
        
        DASSERT(field.is_bit_set(0));
        DASSERT(field.is_bit_set(1));
        DASSERT(field.is_bit_set(2));
        DASSERT(field.is_bit_set(3));
        DASSERT(field.is_bit_set(4));
        DASSERT(field.is_bit_set(5));
    }

    // test overwrite
    {
        field.insert_bits(src, 2, 3, 0);

        DASSERT(field.is_bit_set(0));
        DASSERT(field.is_bit_set(1));
        DASSERT(field.is_bit_set(2));
        DASSERT(!field.is_bit_set(3));
        DASSERT(!field.is_bit_set(4));
        DASSERT(field.is_bit_set(5));
    }

    // test boundary crossing read
    {
        field.reset();
        field.insert_bits(src, 3, 0, 62);

        DASSERT(field.is_bit_set(0));
        DASSERT(field.is_bit_set(1));
        DASSERT(field.is_bit_set(2));
    }

    // test boundary crossing write
    {
        field.reset();
        field.insert_bits(src, 3, 63, 62);

        DASSERT(field.is_bit_set(63));
        DASSERT(field.is_bit_set(64));
        DASSERT(field.is_bit_set(65));
    }
}

void bitrange_tests()
{
    uints bits_left = 0;

    {
        uint8 mask = coid::make_bitmask<uint8>(5, 70, &bits_left);
        DASSERT(mask == 0b11100000);
        DASSERT(bits_left == 67);
    
        mask = coid::make_bitmask<uint8>(5, 2, &bits_left);
        DASSERT(mask == 0b01100000);
        DASSERT(bits_left == 0);

        mask = coid::make_bitmask<uint8>(0, 8, &bits_left);
        DASSERT(mask == 0xff);
        DASSERT(bits_left == 0);
    }

    {
        int8 mask = coid::make_bitmask<int8>(5, 70, &bits_left);
        DASSERT(mask == int8(0b11100000));
        DASSERT(bits_left == 67);

        mask = coid::make_bitmask<int8>(5, 2, &bits_left);
        DASSERT(mask == int8(0b01100000));
        DASSERT(bits_left == 0);

        mask = coid::make_bitmask<int8>(0, 8, &bits_left);
        DASSERT(mask == int8(0xff));
        DASSERT(bits_left == 0);
    }

    {
        uint16 mask = coid::make_bitmask<uint16>(5,70, &bits_left);
        DASSERT(mask == 0b1111111111100000);
        DASSERT(bits_left == 59);

        mask = coid::make_bitmask<uint16>(5, 9, &bits_left);
        DASSERT(mask == 0b0011111111100000);
        DASSERT(bits_left == 0);

        mask = coid::make_bitmask<uint16>(0, 16, &bits_left);
        DASSERT(mask == 0xffff);
        DASSERT(bits_left == 0);
    }

    {
        int16 mask = coid::make_bitmask<int16>(5, 70, &bits_left);
        DASSERT(mask == int16(0b1111111111100000));
        DASSERT(bits_left == 59);

        mask = coid::make_bitmask<int16>(5, 9, &bits_left);
        DASSERT(mask == int16(0b0011111111100000));
        DASSERT(bits_left == 0);

        mask = coid::make_bitmask<int16>(0, 16, &bits_left);
        DASSERT(mask == int16(0xffff));
        DASSERT(bits_left == 0);
    }

    {
        uint32 mask = coid::make_bitmask<uint32>(5, 70, &bits_left);
        DASSERT(mask == 0b11111111'11111111'11111111'11100000ul);
        DASSERT(bits_left == 43);

        mask = coid::make_bitmask<uint32>(5, 25, &bits_left);
        DASSERT(mask == 0b00111111'11111111'11111111'11100000ul);
        DASSERT(bits_left == 0);

        mask = coid::make_bitmask<uint32>(0, 32, &bits_left);
        DASSERT(mask == 0xffffffff);
        DASSERT(bits_left == 0);
    }

    {
        int32 mask = coid::make_bitmask<int32>(5, 70, &bits_left);
        DASSERT(mask == 0b11111111'11111111'11111111'11100000l);
        DASSERT(bits_left == 43);

        mask = coid::make_bitmask<int32>(5, 25, &bits_left);
        DASSERT(mask == 0b00111111'11111111'11111111'11100000l);
        DASSERT(bits_left == 0);

        mask = coid::make_bitmask<int32>(0, 32, &bits_left);
        DASSERT(mask == int32(0xffffffff));
        DASSERT(bits_left == 0);
    }

    {
        uint64 mask = coid::make_bitmask<uint64>(5, 70, &bits_left);
        DASSERT(mask == 0b11111111111111111111111111111111'11111111111111111111111111100000ull);
        DASSERT(bits_left == 11);

        mask = coid::make_bitmask<uint64>(5, 57, &bits_left);
        DASSERT(mask == 0b00111111111111111111111111111111'11111111111111111111111111100000ull);
        DASSERT(bits_left == 0);

        mask = coid::make_bitmask<uint64>(0, 64, &bits_left);
        DASSERT(mask == 0xffffffffffffffff);
        DASSERT(bits_left == 0);
    }

    {
        int64 mask = coid::make_bitmask<int64>(5, 70, &bits_left);
        DASSERT(mask == 0b11111111111111111111111111111111'11111111111111111111111111100000ll);
        DASSERT(bits_left == 11);

        mask = coid::make_bitmask<int64>(5, 57, &bits_left);
        DASSERT(mask == 0b00111111111111111111111111111111'11111111111111111111111111100000ll);
        DASSERT(bits_left == 0);

        mask = coid::make_bitmask<int64>(0, 64, &bits_left);
        DASSERT(mask == int64(0xffffffffffffffff));
        DASSERT(bits_left == 0);
    }
}

void bitfield_tests() 
{
    bitrange_tests();


    uint8 bytes[16] = { 0b11, 0, 0, 0,
                     0, 0, 0, 0,
                     0, 0, 0, 0,
                     0, 0, 0, 0b11000000 };
    coid::range<uint8> r(bytes, sizeof(bytes));
    
    {
        coid::bitfield<16 * 8> field(r);
        DASSERT(field.is_any_set());
        DASSERT(!field.is_none_set());
        DASSERT(field.get_set_bit_count() == 4);
        DASSERT(field.get_unset_bit_count() == 124);
        DASSERT(field.is_bit_set(0));
        DASSERT(field.is_bit_set(1));
        DASSERT(field.is_bit_set(127));
        DASSERT(field.is_bit_set(126));
        //DASSERT(!field.is_bit_set(128));
        
        DASSERT(field.get_first_set_bit_index() == 0);
        DASSERT(field.get_next_set_bit_index(-1) == 0);
        DASSERT(field.get_next_set_bit_index(0) == 1);
        DASSERT(field.get_next_set_bit_index(1) == 126);
        DASSERT(field.get_next_set_bit_index(126) == 127);
        DASSERT(field.get_next_set_bit_index(127) == field.invalid_bit_index);

        DASSERT(field.get_last_set_bit_index() == 127);
        //DASSERT(field.get_prev_set_bit_index(0) == -1);
        DASSERT(field.get_prev_set_bit_index(1) == 0);
        DASSERT(field.get_prev_set_bit_index(2) == 1);
        DASSERT(field.get_prev_set_bit_index(128) == 127);
        DASSERT(field.get_prev_set_bit_index(127) == 126);

        //field.set_bit(-1);
        //field.unset_bit(-1);
        //field.flip_bit(-1);
        
        //field.set_bit(128);
        //field.unset_bit(128);
        //field.flip_bit(128);

        field.set_bit(34);
        DASSERT(field.is_bit_set(34));
        field.unset_bit(34);
        DASSERT(!field.is_bit_set(34));
        field.flip_bit(34);
        DASSERT(field.is_bit_set(34));

        field.set_bit(72);
        DASSERT(field.is_bit_set(72));
        field.unset_bit(72);
        DASSERT(!field.is_bit_set(72));
        field.flip_bit(72);
        DASSERT(field.is_bit_set(72));

        uint32 i = 0;
        field.for_each_bit_set([&](uint32 bit_index) 
        {
            if (i == 0)
            {
                DASSERT(bit_index == 0);
            }

            if (i == 1)
            {
                DASSERT(bit_index == 1);
            }

            if (i == 2)
            {
                DASSERT(bit_index == 34);
            }

            if (i == 3)
            {
                DASSERT(bit_index == 72);
            }

            if (i == 4)
            {
                DASSERT(bit_index == 126);
            }

            if (i == 5)
            {
                DASSERT(bit_index == 127);
            }
            ++i;
        });

        DASSERT(i == field.get_set_bit_count());

        field.reset();
        DASSERT(field.is_none_set());

    }

    test_bitfield_insert_bits();
}