#pragma once
#include "palgorithm.h"
#include "cm_ctors.h"

//this is tricky stuff ...allows to use algorithms like
//std::find_if(OffsetIterator::begin(), OffsetIterator(n),[](size_t index){})

//basicaly it is indexed for loop, however, those algos we can send to openmp easy!

class OffsetIterator : public std::iterator<std::random_access_iterator_tag, size_t>
{
public:

private:
    mutable size_t current;
public:
    DEFAULT_COPYMOVE(OffsetIterator);

    OffsetIterator(size_t val):
        current(val)
    {
    }

    OffsetIterator():
        current(0)
    {
    }

    ~OffsetIterator() = default;

    size_t operator*() const noexcept
    {
        return current;
    }

    OffsetIterator& operator++() noexcept
    {
        // actual increment takes place here
        ++current;
        return *this;
    }

    OffsetIterator operator++(int)noexcept
    {
        OffsetIterator tmp(*this); // copy
        operator++(); // pre-increment
        return tmp;   // return old value
    }

    OffsetIterator& operator--()noexcept
    {
        if (current)
            --current;
        return *this;
    }

    OffsetIterator operator--(int)noexcept
    {
        OffsetIterator tmp(*this);
        operator--();
        return tmp;
    }


    OffsetIterator& operator+=(size_t val)noexcept
    {
        current += val;
        return *this;
    }

    OffsetIterator& operator-=(size_t val)noexcept
    {
        if (current - val > 0)
            current -= val;
        else
            current = 0;
        return *this;
    }

    OffsetIterator operator+(size_t val) const noexcept
    {
        OffsetIterator tmp(*this);
        tmp.current = current + val;
        return tmp;
    }

    OffsetIterator operator-(size_t val) const noexcept
    {
        OffsetIterator tmp(*this);
        tmp.current = current - val;
        return tmp;
    }


    size_t operator+(const OffsetIterator& val) const noexcept
    {
        return current + val.current;
    }

    size_t operator-(const OffsetIterator& val) const noexcept
    {
        return current - val.current;
    }

    bool operator < (const OffsetIterator& c) const noexcept
    {
        return current < c.current;
    }

    bool operator <= (const OffsetIterator& c) const noexcept
    {
        return current <= c.current;
    }

    bool operator > (const OffsetIterator& c) const noexcept
    {
        return current > c.current;
    }

    bool operator >= (const OffsetIterator& c) const noexcept
    {
        return current >= c.current;
    }

    bool operator == (const OffsetIterator& c) const noexcept
    {
        return current == c.current;
    }

    bool operator !=(const OffsetIterator& c) const noexcept
    {
        return !(*this == c);
    }
};
TEST_MOVE_NOEX(OffsetIterator);
