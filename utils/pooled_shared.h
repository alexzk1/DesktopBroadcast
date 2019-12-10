#pragma once
#include <boost/pool/singleton_pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <memory>
#include <atomic>
#include <vector>
#include <deque>
#include <sstream>
#include "type_checks.h"

//allocates shared pointers using boost's pools
namespace pools
{
    using PooledString       = std::basic_string<std::string::value_type, std::string::traits_type, boost::pool_allocator<std::string::value_type>>;
    using PooledStringStream = std::basic_stringstream<PooledString::value_type, PooledString::traits_type, PooledString::allocator_type>;

    template <class T>
    using PooledVector = std::vector<T, boost::pool_allocator<T>>;

    template <class T>
    using PooledDeque = std::deque<T, boost::fast_pool_allocator<T>>;


    template <class T>
    auto pooledVector(size_t inisize = 0)
    {
        PooledVector<T> r;
        r.resize(inisize);
        return r;
    }

    template <class T, class V>
    auto pooledVector(size_t inisize, const V& v)
    {
        PooledVector<T> r;
        r.resize(inisize, v);
        return r;
    }

    //just to make friends easier
    template<class GenT, typename... Args>
    struct AllocShared
    {
        static std::shared_ptr<GenT> alloc(Args&&... args)
        {
            using pool_t = boost::singleton_pool<GenT, sizeof(GenT)>;
            void *mem = pool_t::malloc();
            return std::shared_ptr<GenT>(new (mem) GenT(std::forward<Args>(args)...), [](GenT * p)
            {
                if (p)
                {
                    p->~GenT();
                    pool_t::free(p);
                }
            });
        }

        static std::shared_ptr<GenT> alloc_nm(Args&... args)
        {
            using pool_t = boost::singleton_pool<GenT, sizeof(GenT)>;
            void *mem = pool_t::malloc();
            return std::shared_ptr<GenT>(new (mem) GenT(args...), [](GenT * p)
            {
                if (p)
                {
                    p->~GenT();
                    pool_t::free(p);
                }
            });
        }
    };

    template<class GenT, typename... Args>
    inline std::shared_ptr<GenT> allocShared(Args&&... args)
    {
        return AllocShared<GenT, Args...>::alloc(std::forward<Args>(args)...);
    }

    template<class GenT, typename... Args>
    inline std::shared_ptr<GenT> allocSharedNoMove(Args&... args)
    {
        return AllocShared<GenT, Args...>::alloc_nm(args...);
    }
};

#define DECLARE_FRIEND_POOL    template<class GenT, typename... Args> friend struct pools::AllocShared
