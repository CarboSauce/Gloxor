#pragma once
#include "movesem.hpp"

namespace glox {
GLOX_BEGIN_EXPORT
template <typename T>
constexpr void swap(T& l, T& r)
{
    auto tmp = RVALUE(l);
    l = RVALUE(r);
    r = RVALUE(tmp);
}
template <typename T, typename U = T>
constexpr T exchange(T& l, U&& r)
{
    auto tmp = RVALUE(l);
    l = FORWARD(r);
    return tmp;
}
template <typename First, typename Last, typename Out>
constexpr Out move_range(First first, Last last, Out dest_first)
{
    for (; first != last; ++dest_first, ++first) {
        *dest_first = RVALUE(*first);
    }
    return dest_first;
}
template <typename First, typename Last, typename Out>
constexpr Out move_range_backward(First first, Last last, Out dest_last)
{
    while (first != last)
        *(--dest_last) = RVALUE(*(--last));
    return dest_last;
}
GLOX_END_EXPORT
} // namespace glox
