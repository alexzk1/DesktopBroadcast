#pragma once
#include <stdint.h>
#include <cassert>
#include "cm_ctors.h"
#include "palgorithm.h"
#include "offset_iter.h"

namespace pixel_format
{
    using default_color_type = uint8_t; //how many bits per color

    //pixel iterator, which keeps counting in term of "pixel", which is tripplet of 3 colors
    template <typename ColorT, size_t ColorsPerPixel = 3>
    class PixelIterator : public std::iterator<std::random_access_iterator_tag, ColorT*>
    {
    public:
        static constexpr size_t ColorsPP = ColorsPerPixel;
        using PixelPtr = ColorT*;

    private:
        PixelPtr buffer;
        size_t pixels_amount;
        mutable size_t current;

        PixelIterator(PixelPtr buffer, size_t pixels_amount): //expects size of buffer to be ColorsPerPixel * tripplets_amount of ColotT elements exactly
            buffer(buffer),
            pixels_amount(pixels_amount),
            current(0)
        {
        }

    public:

        DEFAULT_COPYMOVE(PixelIterator);
        PixelIterator():
            buffer(nullptr),
            pixels_amount(0),
            current(0)
        {
        }
        static PixelIterator start(PixelPtr buffer, size_t pixels_amount) noexcept
        {
            return PixelIterator(buffer, pixels_amount);
        }

        static PixelIterator end(PixelPtr buffer, size_t pixels_amount) noexcept
        {
            PixelIterator r(buffer, pixels_amount);
            r.current = pixels_amount;
            return r;
        }

        template<class Container>
        static PixelIterator start(Container& buffer, size_t pixels_amount)
        {
            assert(pixels_amount * ColorsPP <= buffer.size());
            return PixelIterator::start(buffer.data(), pixels_amount);
        }

        template<class Container>
        static PixelIterator end(Container& buffer, size_t pixels_amount)
        {
            assert(pixels_amount * ColorsPP <= buffer.size());
            return PixelIterator::end(buffer.data(), pixels_amount);
        }

        ~PixelIterator() = default;

        PixelPtr pixelAt(size_t n) const noexcept
        {
            if (current + n < pixels_amount)
                return buffer + (current + n) * ColorsPerPixel;
            return nullptr;
        }

        PixelPtr operator*() const noexcept
        {
            return pixelAt(0);
        }

        PixelPtr operator[](size_t n) const noexcept
        {
            return pixelAt(n);
        }

        ColorT& component(size_t n)
        {
            assert(n < ColorsPP);
            return *(pixelAt(0) + n);
        }

        const ColorT& component(size_t n) const
        {
            assert(n < ColorsPP);
            return *(pixelAt(0) + n);
        }

        PixelIterator& operator++() noexcept
        {
            // actual increment takes place here
            ++current;
            return *this;
        }

        PixelIterator operator++(int) noexcept
        {
            PixelIterator tmp(*this); // copy
            operator++(); // pre-increment
            return tmp;   // return old value
        }

        PixelIterator& operator--() noexcept
        {
            if (current)
                --current;
            return *this;
        }

        PixelIterator operator--(int) noexcept
        {
            PixelIterator tmp(*this);
            operator--();
            return tmp;
        }


        PixelIterator& operator+=(size_t val) noexcept
        {
            current += val;
            return *this;
        }

        PixelIterator& operator-=(size_t val) noexcept
        {
            if (current - val > 0)
                current -= val;
            else
                current = 0;
            return *this;
        }

        PixelIterator operator+(size_t val) const noexcept
        {
            PixelIterator tmp(*this);
            tmp.current = current + val;
            return tmp;
        }

        PixelIterator operator-(size_t val) const noexcept
        {
            PixelIterator tmp(*this);
            tmp.current = current - val;
            return tmp;
        }


        size_t operator+(const PixelIterator& val) const noexcept
        {
            return current + val.current;
        }

        size_t operator-(const PixelIterator& val) const noexcept
        {
            return current - val.current;
        }

        bool operator < (const PixelIterator& c) const noexcept
        {
            return current < c.current;
        }

        bool operator <= (const PixelIterator& c) const noexcept
        {
            return current <= c.current;
        }

        bool operator > (const PixelIterator& c) const noexcept
        {
            return current > c.current;
        }

        bool operator >= (const PixelIterator& c) const noexcept
        {
            return current >= c.current;
        }

        bool operator == (const PixelIterator& c) const noexcept
        {
            return buffer == c.buffer && pixels_amount == c.pixels_amount && current == c.current;
        }

        bool operator !=(const PixelIterator& c) const noexcept
        {
            return !(*this == c);
        }

        bool sameImage(const PixelIterator& c) const noexcept
        {
            return buffer == c.buffer;
        }
    };


    using Iterator8888 = PixelIterator<uint8_t, 4>;
    using Iterator888  = PixelIterator<uint8_t, 3>;

    TEST_MOVE_NOEX(Iterator888);
    TEST_MOVE_NOEX(Iterator8888);

    inline void convertBGRA8888_to_RGB888(const Iterator8888& start, const Iterator8888& end, const Iterator888& out)
    {
        assert(start.sameImage(end));
        ALG_NS::for_each(OffsetIterator(0), OffsetIterator(std::distance(start, end)), [&out, &start](size_t index)
        {
            std::copy_n(std::reverse_iterator(start[index] + 3), 3, out[index]);
        });
    }
}
