#pragma once
#include <atomic>
#include <thread>
#include <cassert>

//should have lock()/unlock() same as mutex, using boost for now
//do not use spinlocks for long going operations
//https://habr.com/ru/post/328362/

namespace ns_locks
{
    class spinlock_t
    {
        std::atomic_flag lock_flag;
    public:
        spinlock_t()
        {
            lock_flag.clear();
        }

        bool try_lock() noexcept
        {
            return !lock_flag.test_and_set(std::memory_order_acquire);
        }
        void lock() noexcept
        {
            for (size_t i = 1; !try_lock(); ++i)
                if (i % 100 == 0) std::this_thread::yield();
        }
        void unlock() noexcept
        {
            lock_flag.clear(std::memory_order_release);
        }
    };

    class recursive_spinlock_t
    {
        std::atomic_flag lock_flag;
        int64_t recursive_counter{0};

        typedef std::thread::id thread_id_t;
        std::atomic<std::thread::id> owner_thread_id;
        std::thread::id get_fast_this_thread_id() const noexcept
        {
            return std::this_thread::get_id();
        }
    public:
        recursive_spinlock_t()
        {
            lock_flag.clear();
        }

        bool try_lock() noexcept
        {
            if (!lock_flag.test_and_set(std::memory_order_acquire))
                owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);
            else
            {
                if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id())
                    return false;
            }
            ++recursive_counter;
            return true;
        }

        void lock() noexcept
        {
            for (size_t i = 1; !try_lock(); ++i)
                if (i % 100 == 0) std::this_thread::yield();
        }

        void unlock() noexcept
        {
            assert(owner_thread_id.load(std::memory_order_acquire) == get_fast_this_thread_id());

            const int64_t val = --recursive_counter;
            assert(val > -1);
            if (!val)
            {
                owner_thread_id.store(thread_id_t(), std::memory_order_release);
                lock_flag.clear(std::memory_order_release);
            }
        }
    };
}

using recursive_spinlock = ns_locks::recursive_spinlock_t;
using spinlock = ns_locks::spinlock_t;
