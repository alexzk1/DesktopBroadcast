#pragma once
#include <memory>
#include <type_traits>
#include <set>
#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace types_ns
{
    template <class T, class S>
    bool isTypePtr(S* ptr)
    {
        return dynamic_cast<T*>(ptr) != nullptr;
    }

    template <class T, class S>
    bool isTypePtr(const S* ptr)
    {
        return dynamic_cast<const T*>(ptr) != nullptr;
    }

    template <class T, class S>
    bool isTypePtr(const std::shared_ptr<S>& ptr)
    {
        return std::dynamic_pointer_cast<T>(ptr) != nullptr;
    }




    //some templates to make proper conditional compiling with explicit instantiation

    template <typename T> using        isarray = typename std::is_array<T>::type;
    template <typename T>       struct isset: std::false_type {};
    template <typename... Args> struct isset<std::set<Args...>>: std::true_type {};

    template <typename T>       struct ismap: std::false_type {};
    template <typename T>       struct isqtmap: std::false_type {};

    template <typename... Args> struct ismap<std::map<Args...>>: std::true_type {};
    template <typename... Args> struct ismap<std::unordered_map<Args...>>: std::true_type {};

    template <typename T>       struct ispair: std::false_type {};
    template <typename... Args> struct ispair<std::pair<Args...>>: std::true_type {};


    template <typename T>       struct isvector: std::false_type {};
    template <typename... Args> struct isvector<std::vector<Args...>>: std::true_type {};

    template <typename T>       struct issharedptr: std::false_type {};
    template <typename... Args> struct issharedptr<std::shared_ptr<Args...>>: std::true_type {};

    template <typename T>       struct isweakptr: std::false_type {};
    template <typename... Args> struct isweakptr<std::weak_ptr<Args...>>: std::true_type {};


    template <typename T>       struct islockguard: std::false_type {};
    template <typename... Args> struct islockguard<std::lock_guard<Args...>>: std::true_type {};

    template <class T, size_t N>
    struct TestAlignForStruct
    {
        using is_align1 = typename std::conditional<alignof (T) == N, std::true_type, std::false_type>::type;
        static constexpr bool value{std::conditional<std::is_class<T>::value, is_align1, std::true_type>::type::value};
    };

    template <class T>
    using TestAlign1ForStruct = TestAlignForStruct<T, 1>;


    //taken from here: https://stackoverflow.com/questions/15393938/find-out-whether-a-c-object-is-callable
    template<typename T, typename U = void>
    struct is_callable
    {
        static bool const constexpr value = std::conditional_t <
                                            std::is_class<std::remove_reference_t<T>>::value,
                                            is_callable<std::remove_reference_t<T>, int>, std::false_type >::value;
    };

    template<typename T, typename U, typename ...Args>
struct is_callable<T(Args...), U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(*)(Args...), U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(&)(Args...), U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(Args......), U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(*)(Args......), U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(&)(Args......), U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(Args...)const, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(Args...)volatile, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable<T(Args...)const volatile, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(Args......)const, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(Args......)volatile, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable<T(Args......)const volatile, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(Args...)&, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable<T(Args...)const&, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable<T(Args...)volatile&, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable<T(Args...)const volatile&, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable<T(Args......)&, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable<T(Args......)const&, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable<T(Args......)volatile&, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable<T(Args......)const volatile&, U> : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable < T(Args...)&&, U > : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable < T(Args...)const&&, U > : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable < T(Args...)volatile &&, U > : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable < T(Args...)const volatile &&, U > : std::true_type {};
    template<typename T, typename U, typename ...Args>
struct is_callable < T(Args......) &&, U > : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable < T(Args......)const &&, U > : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable < T(Args......)volatile &&, U > : std::true_type {};
    template<typename T, typename U, typename ...Args>
    struct is_callable < T(Args......)const volatile &&, U > : std::true_type {};

    template<typename T>
    struct is_callable<T, int>
    {
        using YesType = char(&)[1];
        using NoType = char(&)[2];

        struct Fallback
        {
            void operator()();
        };

        struct Derived : T, Fallback {};

        template<typename U, U>
        struct Check;

        template<typename>
        static YesType Test(...);

        template<typename C>
        static NoType Test(Check<void (Fallback::*)(), &C::operator()>*);

        static bool const constexpr value = sizeof(Test<Derived>(0)) == sizeof(YesType);
    };

};

#define VALUE_TYPE(ARR) std::enable_if<std::is_array<decltype(ARR)>::value, std::decay<decltype(*ARR)>::type>::type
