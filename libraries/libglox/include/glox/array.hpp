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
	static constexpr size_t size = N;
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
	constexpr auto begin() {return data;}
	constexpr auto begin() const {return data;}
	constexpr auto end() {return data + N;}
	constexpr auto end() const {return data + N;}
	constexpr auto& back() { return data[N-1];}
	constexpr auto& back() const { return data[N-1];}
	constexpr auto& front() const { return data[0];}
	constexpr auto& front() { return data[0];}
};
} // namespace glox
