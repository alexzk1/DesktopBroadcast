#ifndef CONDITIONAL_WAIT_H
#define CONDITIONAL_WAIT_H
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <cassert>
#include "cm_ctors.h"
#include "atomic_lock.h"
//this one ensures that "confirm" will not be missed IF confirm() happened before waitConfirm()
//(that is nothing about password, pass = "passing")
namespace thread_sync
{
    class confirmed_pass
    {
    private:
        using Mutex = std::mutex; //damn, conditional_variable uses ONLY std:mutex (hardcoded)
        std::unique_ptr<Mutex>                   waitMtx;
        std::unique_ptr<std::condition_variable> cv;
        bool                                     conf{false}; //"Effective C++ 14" - no need in atomical when is guarded by mutex

    public:
        using wait_lambda = std::function<bool()>;
        confirmed_pass(): waitMtx(new Mutex()), cv(new std::condition_variable())
        {
        }
        ~confirmed_pass() = default;

        NO_COPYMOVE(confirmed_pass);

        void waitConfirm()
        {
            std::unique_lock<Mutex> lck(*waitMtx);
            if (!conf)
                cv->wait(lck, [this]()->bool{return conf;});
            conf = false;
        }

        void confirm()
        {
            std::unique_lock<Mutex> lck(*waitMtx) ;
            conf = true;
            cv->notify_all();
        }
    };

    //allows to limit amount of threads which pass lock
    class limited_pass
    {
    private:
        confirmed_pass pass;
        const size_t max;
        std::atomic<size_t> counter{0};
    public:
        limited_pass(size_t max) : max(max) {}
        void lock()
        {
            while (true)
            {
                const auto old = counter++;
                if (old < max)
                    break;
                --counter;
                pass.waitConfirm();
            }
        }
        void unlock()
        {
            const auto old = counter--;
            pass.confirm();

            assert(old > 0);
            (void)old;
        }
    };
};
#endif // CONDITIONAL_WAIT_H
