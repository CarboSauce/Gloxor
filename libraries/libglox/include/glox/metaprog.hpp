#pragma once
#include "detail/movesem.hpp"

#ifndef USE_MODULES
#include <cstddef>
#include <initializer_list>
#endif

namespace glox {
GLOX_BEGIN_EXPORT
template <typename T, T... I>
struct integer_sequence
{
    using value_type = T;
    static constexpr auto size()
    {
        return sizeof...(I);
    }
};
template <std::size_t... I>
using index_sequence = integer_sequence<std::size_t, I...>;

template <std::size_t I>
using make_index_sequence =
#if __has_builtin(__make_integer_seq)
    __make_integer_seq<integer_sequence, std::size_t, I>;
#else
    index_sequence<__integer_pack(I)...>;
#endif
template <class... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

template <typename T>
constexpr T* launder(T* p) noexcept
{
    return __builtin_launder(p);
}
GLOX_END_EXPORT
} // namespace glox
