#pragma once
#include "glox/assert.hpp"
#include "glox/types.hpp"
#include <type_traits>

namespace glox
{
template <typename T, size_t N>
struct array
{
	T data[N];
	constexpr const T& operator[](size_t i) const
	{
		gloxAssert(i < N, "Out of bounds access");
		return data[i];
	}
	constexpr T& operator[](size_t i)
	{
		return const_cast<T&>(static_cast<const array<T, N>&>(*this)[i]);
	}
	constexpr explicit operator T*() { return data; }
	constexpr explicit operator const T*() const { return data; }
	constexpr size_t size() const
	{
		return N;
	}
};
} // namespace glox