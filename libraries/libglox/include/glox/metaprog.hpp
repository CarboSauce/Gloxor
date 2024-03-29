#pragma once
#include "detail/movesem.hpp"
#include <type_traits>

namespace std
{
template <typename T>
struct tuple_size;
template <size_t S, typename T>
struct tuple_element;
} // namespace std
namespace glox
{
template <typename T, T... I>
struct integer_sequence
{
	using value_type = T;
	static constexpr auto size() { return sizeof...(I); }
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
} // namespace glox

#if __has_builtin(__builtin_bit_cast)
	#define BITCAST(T, from) __builtin_bit_cast(T, from)
#else
	#error Missing support for __builtin_bit_cast
#endif
