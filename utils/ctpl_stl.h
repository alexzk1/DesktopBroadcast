/*********************************************************
*
*  Copyright (C) 2014 by Vitaliy Vitsentiy
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*********************************************************/


#ifndef __ctpl_stl_thread_pool_H__
#define __ctpl_stl_thread_pool_H__

#include <functional>
#include <boost/thread.hpp>
#include <atomic>
#include <vector>
#include <memory>
#include <exception>
#include <future>
#include <mutex>
#include <queue>

// thread pool to run user's functors with signature
//      ret func(int id, other_params)
// where id is the index of the thread that runs the functor
// ret is some return type

namespace ctpl
{

    namespace detail
    {
        template <typename T>
        class Queue
        {
        public:
            bool push(T const & value)
            {
                std::unique_lock<std::mutex> lock(this->mutex);
                this->q.push(value);
                return true;
            }

            //            T& push(T&& value)&
            //            {
            //                std::unique_lock<std::mutex> lock(this->mutex);
            //                this->q.push(std::move(value));
            //                return this->q.back();
            //            }

            // deletes the retrieved element, do not use for non integral types
            bool pop(T & v)
            {
                std::unique_lock<std::mutex> lock(this->mutex);
                if (this->q.empty())
                    return false;
                v = this->q.front();
                this->q.pop();
                return true;
            }

            //            T pop()
            //            {
            //                std::unique_lock<std::mutex> lock(this->mutex);
            //                if (this->q.empty())
            //                    return T();
            //                T v(std::move(this->q.front()));
            //                this->q.pop();
            //                return v;
            //            }

            bool empty()
            {
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->q.empty();
            }
        private:
            std::queue<T> q;
            std::mutex mutex;
        };
    }


    typedef std::function<void (int)> executor_t;
    typedef std::function<bool ()> canrun_t;
    typedef std::pair<executor_t*, canrun_t* > fpair;

    namespace thread_ns = std;
    class thread_pool
    {

    public:
        thread_pool()
        {
            this->init();
        }
        explicit thread_pool(int nThreads)
        {
            this->init();
            this->resize(nThreads);
        }

        ~thread_pool()
        {
            this->stop(false); //dont wait - abort all
        }

        // get the number of running threads in the pool
        int size() const
        {
            return static_cast<int>(this->threads.size());
        }

        // number of idle threads
        int n_idle() const
        {
            return this->nWaiting;
        }
        thread_ns::thread & get_thread(int i) const
        {
            return *this->threads[i];
        }
        // change the number of threads in the pool
        // should be called from one thread, otherwise be careful to not interleave, also with this->stop()
        // nThreads must be >= 0


        void resize(int nThreads)
        {
            if (!this->isStop && !this->isDone)
            {
                int oldNThreads = static_cast<int>(this->threads.size());
                if (oldNThreads <= nThreads)    // if the number of threads is increased
                {
                    this->threads.resize(nThreads);
                    this->flags.resize(nThreads);

                    for (int i = oldNThreads; i < nThreads; ++i)
                    {
                        this->flags[i] = std::make_shared<std::atomic<bool>>(false);
                        this->set_thread(i);
                    }
                }
                else
                {
                    // the number of threads is decreased
                    for (int i = oldNThreads - 1; i >= nThreads; --i)
                    {
                        *this->flags[i] = true;  // this thread will finish
                        this->threads[i]->detach();
                    }
                    {
                        // stop the detached threads that were waiting
                        std::unique_lock<std::mutex> lock(this->mutex);
                        this->cv.notify_all();
                    }
                    this->threads.resize(nThreads);  // safe to delete because the threads are detached
                    this->flags.resize(nThreads);  // safe to delete because the threads have copies of shared_ptr of the flags, not originals
                }
            }
        }

        void reInit()
        {
            int oldNThreads = static_cast<int>(this->threads.size());
            clear_queue();
            stop(false);
            resize(0);
            init();
            resize(oldNThreads);
        }


        // empty the queue
        void clear_queue()
        {
            fpair _f;
            while (this->q.pop(_f))
            {
                delete _f.first; // empty the queue
                delete _f.second;
            }
        }

        // wait for all computing threads to finish and stop all threads
        // may be called asynchronously to not pause the calling thread while waiting
        // if isWait == true, all the functions in the queue are run, otherwise the queue is cleared without running the functions
        void stop(bool isWait = false)
        {
            isStopRequested = true;
            if (!isWait)
            {
                if (this->isStop)
                    return;
                this->isStop = true;
                for (int i = 0, n = this->size(); i < n; ++i)
                {
                    *this->flags[i] = true;  // command the threads to stop
                }
                this->clear_queue();  // empty the queue
            }
            else
            {
                if (this->isDone || this->isStop)
                    return;
                this->isDone = true;  // give the waiting threads a command to finish
            }
            {
                std::unique_lock<std::mutex> lock(this->mutex);
                this->cv.notify_all();  // stop all waiting threads
            }
            for (int i = 0; i < static_cast<int>(this->threads.size()); ++i)
            {
                // wait for the computing threads to finish
                if (this->threads[i]->joinable())
                    this->threads[i]->join();
            }
            // if there were no threads in the pool but some functors in the queue, the functors are not deleted by the threads
            // therefore delete them here
            this->clear_queue();
            this->threads.clear();
            this->flags.clear();
        }

        // run the user's function that excepts argument int - id of the running thread. returned value is templatized
        // operator returns std::future, where the user can get the result and rethrow the catched exceptins

        auto push(executor_t && f, canrun_t && r) ->std::future<void>
        {
            auto pck = std::make_shared<std::packaged_task<void(int)>>(std::forward<executor_t>(f));
            auto _f = new executor_t([pck](int id)
            {
                (*pck)(id);
            });

            //just a copy - not a "packed_task" bcs it can run once ...so canrun must be thread-safe
            auto _r = new canrun_t(r);
            this->q.push(std::make_pair(_f, _r));
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_one();
            return pck->get_future();
        }

        bool stopped() const
        {
            return this->isStopRequested;
        }

    private:

        // deleted
        thread_pool(const thread_pool &) = delete;
        thread_pool(thread_pool &&) = delete;
        thread_pool & operator=(const thread_pool &) = delete;
        thread_pool & operator=(thread_pool &&) = delete;

        void set_thread(int i)
        {
            std::shared_ptr<std::atomic<bool>> flag(this->flags[i]); // a copy of the shared ptr to the flag
            auto f = [this, i, flag/* a copy of the shared ptr to the flag */]()
            {
                std::atomic<bool> & _flag = *flag;
                fpair fr;
                bool isPop = this->q.pop(fr);
                while (true)
                {
                    while (isPop)
                    {
                        bool r = (*fr.second)();
                        if (r) //can it be executed now?
                        {
                            // if there is anything in the queue
                            std::unique_ptr<executor_t> func(fr.first); // at return, delete the function even if an exception occurred
                            std::unique_ptr<canrun_t>   funcr(fr.second);
                            (*func)(i);
                            if (_flag)
                                return;  // the thread is wanted to stop, return even if the queue is not empty yet
                        }
                        else
                        {
                            this->q.push(fr); //return it back
                        }
                        isPop = this->q.pop(fr);
                    }
                    // the queue is empty here, wait for the next command
                    std::unique_lock<std::mutex> lock(this->mutex);
                    ++this->nWaiting;
                    this->cv.wait(lock, [this, &fr, &isPop, &_flag]()
                    {
                        isPop = this->q.pop(fr);
                        return isPop || this->isDone || _flag;
                    });
                    --this->nWaiting;
                    if (!isPop)
                        return;  // if the queue is empty and this->isDone == true or *flag then return
                }
            };
            //            boost::thread::attributes attr;
            //            attr.set_stack_size(1024 * 1024 * 8);
            //this->threads[i].reset(new thread_ns::thread(attr, f)); // compiler may not support std::make_unique()
            this->threads[i].reset(new thread_ns::thread(f)); // compiler may not support std::make_unique()
        }

        void init()
        {
            this->nWaiting = 0;
            this->isStop = false;
            this->isDone = false;
            isStopRequested = false;
        }

        std::vector<std::unique_ptr<thread_ns::thread>> threads;
        std::vector<std::shared_ptr<std::atomic<bool>>> flags;
        detail::Queue<fpair> q;
        std::atomic<bool> isDone;
        std::atomic<bool> isStop;
        std::atomic<bool> isStopRequested;
        std::atomic<int>  nWaiting;  // how many threads are waiting

        std::mutex mutex;
        std::condition_variable cv;
    };

    inline thread_pool& getGlobalPool()
    {
        static thread_pool pool(std::thread::hardware_concurrency());
        return pool;
    }

    template <class Cont, class Callback>
    void inline ForEachParallel(Cont& cont, const Callback& todo)
    {
        std::atomic<size_t> totaldone{0};

        std::atomic<size_t> counter{0};
        const size_t slice = cont.size() /  std::thread::hardware_concurrency() + 1;
        for (auto it = cont.begin(); it != cont.end();)
        {
            const size_t dist = std::min<size_t>(std::distance(it, cont.end()), slice);
            auto itend = it;
            std::advance(itend, dist);
            ++counter;
            getGlobalPool().push([&counter, todo, it, itend, &totaldone](int tid)
            {
                //auto sz = std::distance(it, itend);
                //std::cout << "Thread ID: " << tid << ", addresses to work on: " << sz << std::endl;
                try
                {
                    for (auto i = it; i != itend; ++i)
                    {
                        todo(*i);
                        ++totaldone;
                    }
                }
                catch (...)
                {
                }
                --counter;
            }, []()
            {
                return true;
            });

            it = itend;
        }

        while (counter)
            ;//std::this_thread::sleep_for(std::chrono::nanoseconds(10));
        assert(totaldone == cont.size());
    }
}

inline std::thread::id mainThread()
{
    //must be called from main() initially
    const static std::thread::id id = std::this_thread::get_id();
    return id;
}



#endif // __ctpl_stl_thread_pool_H__
