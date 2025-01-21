#include "uid.h"

#include "../rnd.h"
#include "../timer.h"

coid::charstr coid::generate_unique_id()
{
    coid::rnd_strong rnd(coid::nsec_timer::current_time_ns());
    uint val[4];
    rnd.nrand(4, val);

    coid::charstr result;
    constexpr coid::token_literal table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"_T;

    for (uint v : val)
    {
        constexpr uint8 mask_6_bit = 0x3f;
        constexpr uint8 shift_6_bit = 6;

        for (uint8 i = 0; i < 5; ++i)
        {
            result << table[v & mask_6_bit];
            v = v >> shift_6_bit;
        }
    }

    return result;
}
