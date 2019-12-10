#pragma once
#include <array>

namespace marrays
{
    //this wrapper does index check on [] and accepts enum class as indexes
    template <typename T, std::size_t J>
    struct array_wrapper
    {
    private:
        template <class I>
        constexpr size_t icast(const I& val, std::true_type) const
        {
            return static_cast<size_t>(cast_enum(val));
        }

        template <class I>
        constexpr size_t icast(const I& val, std::false_type) const
        {
            return static_cast<size_t>(val);
        }

    public:
        using array_t    = std::array <T, J>;
        using value_type = T;
        array_t arr;

        template <class I>
        constexpr auto& operator[](const I index)
        {
            using en = std::is_enum<I>;
            static_assert(std::is_integral<I>::value || en::value, "Accepting integral values only.");

#ifdef NDEBUG
            return arr[icast(index, en {})];
#else
            return arr.at(icast(index, en {}));
#endif
        }

        template <class I>
        constexpr const auto& operator[](const I index) const
        {
            using en = std::is_enum<I>;
            static_assert(std::is_integral<I>::value || en::value, "Accepting integral values only.");

#ifdef NDEBUG
            return arr[icast(index, en {})];
#else
            return arr.at(icast(index, en {}));
#endif
        }

        explicit constexpr operator array_t&() noexcept
        {
            return arr;
        }

        explicit constexpr operator const array_t&() const noexcept
        {
            return arr;
        }

        constexpr typename array_t::iterator begin() noexcept
        {
            return arr.begin();
        }

        constexpr typename array_t::const_iterator begin() const noexcept
        {
            return arr.begin();
        }

        constexpr typename array_t::iterator end() noexcept
        {
            return arr.end();
        }

        constexpr typename array_t::const_iterator end() const noexcept
        {
            return arr.end();
        }

        constexpr typename array_t::size_type size() const noexcept
        {
            return arr.size();
        }
    };
}

template <class T, size_t N>
using array1d = marrays::array_wrapper<T, N>;

template <class T, size_t N, size_t M>
using array2d = array1d<array1d<T, M>, N>;


namespace marrays
{
    //Usage: auto arr = marrays::array_of<int>(0,1,2,3,4,5);
    //is equvalent of: int arr[] = {0,1,2,3,4,5,};
    //except it does 1d array, not C-style

    template <typename V, typename... T>
    constexpr auto array_of(T&&... t) -> array1d<V, sizeof...(T)>
    {
        return {{ std::forward<T>(t)... }};
    }
}
