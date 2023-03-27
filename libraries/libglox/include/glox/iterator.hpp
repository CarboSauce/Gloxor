#pragma once
#ifndef _NEW
	#define _NEW
#endif
#include <bits/stl_iterator.h>
#include <bits/stl_iterator_base_funcs.h>
#include <bits/stl_iterator_base_types.h>
namespace glox
{
template <typename T>
[[gnu::always_inline]] constexpr decltype(auto) begin(T& t) { return t.begin(); }
template <typename T>
[[gnu::always_inline]] constexpr decltype(auto) end(T& t) { return t.end(); }
template <typename T>
[[gnu::always_inline]] constexpr decltype(auto) begin(const T& t) { return t.begin(); }
template <typename T>
[[gnu::always_inline]] constexpr decltype(auto) end(const T& t) { return t.end(); }
} // namespace glox
