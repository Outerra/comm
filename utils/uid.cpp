#include "uid.h"

#include "../rnd.h"
#include "../timer.h"

static constexpr coid::token_literal uid_char_table_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"_T;
static constexpr uint32 uid_str_len = 20;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::charstr coid::generate_unique_id()
{
    coid::rnd_strong rnd(coid::nsec_timer::current_time_ns());
    uint val[4];
    rnd.nrand(4, val);

    coid::charstr result;

    for (uint v : val)
    {
        constexpr uint8 mask_6_bit = 0x3f;
        constexpr uint8 shift_6_bit = 6;

        for (uint8 i = 0; i < 5; ++i)
        {
            result << uid_char_table_table[v & mask_6_bit];
            v = v >> shift_6_bit;
        }
    }

    return result;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bool coid::is_valid_unique_id(const coid::token& uid)
{
    return uid.len() == 20 && uid.count_ingroup(uid_char_table_table) == uid_str_len;
}
