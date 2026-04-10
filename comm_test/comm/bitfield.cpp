#include <comm/bitfield.h>

void bitfield_tests() 
{
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
        DASSERT(field.get_next_set_bit_index(127) == -1);

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
}