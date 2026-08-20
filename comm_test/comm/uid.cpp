#include <comm/utils/uid.h>

void run_uid_tests() 
{
    coid::charstr uid = coid::generate_unique_id();
    
    DASSERTX(coid::is_valid_unique_id(uid), "Test valid unique id");

    uid[uid.len() - 1] = '/';

    DASSERTX(!coid::is_valid_unique_id(uid), "Test non valid unique id. Contains invalid character");
    
    uid.resize(18);
    DASSERTX(!coid::is_valid_unique_id(uid), "Test non valid unique id. Wrong character count.");
    
    uid.append("abcd");
    DASSERTX(!coid::is_valid_unique_id(uid), "Test non valid unique id. Wrong character count.");

    uid.reset();
    DASSERTX(!coid::is_valid_unique_id(uid), "Test non valid unique id. Wrong character count.");
}