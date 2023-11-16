#include "condition_variable.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace coid;

condition_variable::condition_variable()
{
    InitializeConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(&_cv));
}

condition_variable::~condition_variable()
{
}


void condition_variable::notify_one()
{
    WakeConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(&_cv));
}

void condition_variable::notify_all()
{
    WakeAllConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(&_cv));
}

void condition_variable::wait(_comm_mutex& mx)
{
    SleepConditionVariableCS(reinterpret_cast<CONDITION_VARIABLE*>(&_cv), reinterpret_cast<CRITICAL_SECTION*>(&mx), 0);
}

bool condition_variable::wait_for(_comm_mutex& mx, uint ms)
{
    return SleepConditionVariableCS(reinterpret_cast<CONDITION_VARIABLE*>(&_cv), reinterpret_cast<CRITICAL_SECTION*>(&mx), ms);
}

