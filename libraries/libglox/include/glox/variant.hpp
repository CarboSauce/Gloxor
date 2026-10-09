#pragma once
#include "assert.hpp"
#include "detail/movesem.hpp"
#include "metaprog.hpp"
#include <type_traits>

namespace glox {

inline constexpr std::size_t npos = -1;

template <class... Ts>
class variant
{
    template <class T>
    inline static constexpr std::size_t index_of_v = [] {
        std::size_t i = 0;
        (void)((std::is_same_v<T, Ts> || (++i, false)) || ...);
        return i == sizeof...(Ts) ? npos : i;
    }();

    inline static constexpr bool all_unique_v
        = []<std::size_t... I>(glox::index_sequence<I...>) {
              return ((index_of_v<Ts> == I) && ...);
          }(glox::index_sequence_for<Ts...> { });

    inline static constexpr std::size_t max_sizeof_v = [] {
        std::size_t r = 0;
        ((r = sizeof(Ts) > r ? sizeof(Ts) : r), ...);
        return r;
    }();
    static constexpr std::size_t N = sizeof...(Ts);
    static_assert(N > 0 && all_unique_v, "need distinct alternatives");
    static_assert(
        std::is_constructible_v<Ts...[0]>,
        "alternative 0 must be default-constructible"
    );

    template <std::size_t I>
    using alt_t = Ts...[I];

    alignas(Ts...) unsigned char buf_[max_sizeof_v];
    std::conditional_t<(N < 256), unsigned char, std::size_t> idx_;

    template <std::size_t I>
    constexpr auto* ptr(this auto&& self)
    {
        using T = alt_t<I>;
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>)
            return glox::launder(
                static_cast<const T*>(static_cast<const void*>(self.buf_))
            );
        else
            return glox::launder(
                static_cast<T*>(static_cast<void*>(self.buf_))
            );
    }

    template <class F>
    constexpr decltype(auto) visit_at(this auto&& self, F&& f)
    {
        using R = decltype(f.template operator()<0>(*self.template ptr<0>()));

#define CASE(i)                                                   \
    case i:                                                       \
        if constexpr (i < N)                                      \
            return static_cast<R>(                                \
                f.template operator()<i>(*self.template ptr<i>()) \
            );                                                    \
        else                                                      \
            GLOX_UNREACHABLE();

        switch (self.idx_) {
            CASE(0)
            CASE(1)
            CASE(2)
            CASE(3)
            CASE(4)
            CASE(5)
            CASE(6)
            CASE(7)
            CASE(8)
            CASE(9)
            CASE(10)
            CASE(11)
            CASE(12)
            CASE(13)
            CASE(14)
            CASE(15)
        default:
            if constexpr (N > 16)
                return self.visit_at(f);
            else
                GLOX_UNREACHABLE();
        }
#undef CASE
    }

    constexpr void destroy()
    {
        if constexpr (!(std::is_trivially_destructible_v<Ts> && ...))
            visit_at([]<std::size_t I>(auto& x) {
                using T = alt_t<I>;
                x.~T();
            });
    }

    template <std::size_t I, class... A>
    constexpr void construct(A&&... a)
    {
        ::new (static_cast<void*>(buf_)) alt_t<I>(FORWARD(a)...);
        idx_ = I;
    }

    static constexpr bool TRIVIAL_COPY
        = ((std::is_trivially_constructible_v<Ts, const Ts&>
               && std::is_trivially_assignable_v<Ts&, const Ts&>)
            && ...);
    static constexpr bool TRIVIAL_MOVE
        = ((std::is_trivially_constructible_v<Ts, Ts&&>
               && std::is_trivially_assignable_v<Ts&, Ts&&>)
            && ...);
    static constexpr bool TRIVIAL_DTOR
        = (std::is_trivially_destructible_v<Ts> && ...);

public:
    constexpr variant()
    {
        construct<0>();
    }

    template <class U>
        requires(index_of_v<std::remove_cvref_t<U>> != npos)
             && (!__is_same(std::remove_cvref_t<U>, variant))
    constexpr variant(U&& u)
    {
        construct<index_of_v<std::remove_cvref_t<U>>>(FORWARD(u));
    }

    // copy
    constexpr variant(const variant&)
        requires TRIVIAL_COPY&& TRIVIAL_DTOR
    = default;
    constexpr variant& operator=(const variant&)
        requires TRIVIAL_COPY&& TRIVIAL_DTOR
    = default;
    constexpr variant(const variant& o)
    {
        o.visit_at([this]<std::size_t I>(const auto& x) { construct<I>(x); });
    }
    constexpr variant& operator=(const variant& o)
    {
        if (this != &o) {
            destroy();
            new (this) variant(o);
        }
        return *this;
    }

    // move
    constexpr variant(variant&&)
        requires TRIVIAL_MOVE&& TRIVIAL_DTOR
    = default;
    constexpr variant& operator=(variant&&)
        requires TRIVIAL_MOVE&& TRIVIAL_DTOR
    = default;
    constexpr variant(variant&& o)
    {
        o.visit_at([this]<std::size_t I>(auto& x) { construct<I>(RVALUE(x)); });
    }
    constexpr variant& operator=(variant&& o)
    {
        if (this != &o) {
            destroy();
            new (this) variant(RVALUE(o));
        }
        return *this;
    }

    // destructor
    constexpr ~variant()
        requires TRIVIAL_DTOR
    = default;
    constexpr ~variant()
    {
        destroy();
    }

    template <std::size_t I, class... A>
        requires(I < N) && __is_constructible
    (alt_t<I>, A...) constexpr alt_t<I>& emplace(A&&... a)
    {
        destroy();
        construct<I>(FORWARD(a)...);
        return *ptr<I>();
    }
    template <class T, class... A>
        requires(index_of_v<T> != npos)
    constexpr T& emplace(A&&... a)
    {
        return emplace<index_of_v<T>>(FORWARD(a)...);
    }

    constexpr std::size_t index() const
    {
        return idx_;
    }

    template <std::size_t I>
    constexpr auto* get_if(this auto&& self)
    {
        return self.idx_ == I ? self.template ptr<I>() : nullptr;
    }
    template <class T>
    constexpr auto* get_if(this auto&& self)
    {
        return self.template get_if<index_of_v<T>>();
    }

    template <class F>
    constexpr decltype(auto) visit(this auto&& self, F&& f)
    {
        return self.visit_at([&]<std::size_t>(auto& x) -> decltype(auto) {
            return f(x);
        });
    }
};

template <typename... Ts>
struct overload : Ts...
{
    using Ts::operator()...;
};

template <typename... Ts>
overload(Ts...) -> overload<Ts...>;

} // namespace glox
