#ifndef PALGORITHM_H
#define PALGORITHM_H

#include <algorithm>
#include <atomic>

#ifdef USING_OPENMP
#include <parallel/algorithm>

#ifndef ALG_NS
#define ALG_NS __gnu_parallel
#endif

#endif

//fallback to std
#ifndef ALG_NS
#define ALG_NS std
#endif


template<typename T>
void update_maximum(std::atomic<T>& maximum_value, T const& value) noexcept
{
    T prev_value = maximum_value;
    while(prev_value < value &&
            !maximum_value.compare_exchange_weak(prev_value, value))
        ;
}

template<typename T>
void update_minimum(std::atomic<T>& maximum_value, T const& value) noexcept
{
    T prev_value = maximum_value;
    while(prev_value > value &&
            !maximum_value.compare_exchange_weak(prev_value, value))
        ;
}

#endif // PALGORITHM_H
