#ifndef LIBGLOX_MOVEUTILS
#define LIBGLOX_MOVEUTILS
#include "movesem.hpp"

namespace glox
{
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
} // namespace glox
#endif
