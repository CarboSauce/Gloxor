#pragma once
#include "glox/assert.hpp"
#include "glox/types.hpp"


namespace glox
{
	template <typename T, size_t N>
	struct array
	{
		T data[N];
		constexpr T& operator[](size_t i)
		{
			gloxAssert(i > 0 && i < N, "Out of bounds access");
			return data[i];
		}
		constexpr const T& operator[](size_t i) const
		{
			gloxAssert(i > 0 && i < N, "Out of bounds access");
			return data[i];
		}
		constexpr explicit operator T*() { return data; }
		constexpr explicit operator const T*() const { return data; }
		constexpr size_t size()
		{
			return N;
		}
		
	};
} // namespace glox