#pragma once
#include <atomic>
#include <mutex>
#include "spinlock.h"
#include <type_traits>
#include "exec_exit.h"
#include "cm_ctors.h"

#define LOCK_GUARD_ON(MUTEX_NAME) std::lock_guard<std::decay<decltype(MUTEX_NAME)>::type> __guard_var##MUTEX_NAME(MUTEX_NAME)

//inside templates use this one macro, it adds "typename"
#define LOCK_GUARD_ON_TEMPL(MUTEX_NAME) std::lock_guard<typename std::decay<decltype(MUTEX_NAME)>::type> __guard_var##MUTEX_NAME(MUTEX_NAME)

//tries to lock mutex, doing Predicate as weel, if pred() returns true it stops attempts to lock
template <class Mutex, class Predicate>
class lock_guard_conditional
{
private:
    Mutex& mut;
    Predicate& pred;
    bool locked{false};
public:
    lock_guard_conditional() = delete;
    NO_COPYMOVE(lock_guard_conditional);
    NO_NEW;

    lock_guard_conditional(Mutex& mut, Predicate& pred):
        mut(mut), pred(pred)
    {
        for (size_t i = 1; !(locked = this->mut.try_lock()); ++i)
            if (i % 100 == 0)
            {
                if (pred())
                    break;
                std::this_thread::yield();
            }
    }
    ~lock_guard_conditional()
    {
        if (locked)
            mut.unlock();
    }

    bool isLocked() const
    {
        return locked;
    }
};
