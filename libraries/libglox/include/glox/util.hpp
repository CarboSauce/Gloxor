#pragma once
#include "assert.hpp"
#include "type_traits"
#include "types.hpp"
#include "metaprog.hpp"

namespace glox
{
template <typename T, typename U>
struct pair
{
	T first;
	U second;
	constexpr friend auto operator<=>(const pair&, const pair&) = default;
};

template <typename T>
struct vec2
{
	T x;
	T y;
	// constexpr friend auto operator<=>(const vec2&,const vec2&) = default;
};

template <typename T>
class span
{
	T *from, *to;

 public:
	using iterator = T*;
	constexpr span() : from(nullptr), to(nullptr) {}
	constexpr span(T* from, T* to) : from(from), to(to)
	{
		gloxAssert(this->from < this->to, "Span shouldn't have negative range!");
	}
	[[nodiscard]] constexpr iterator begin() const { return from; }
	[[nodiscard]] constexpr iterator end() const { return to; }
	[[nodiscard]] constexpr size_t size() const { return to - from; }
	constexpr const T& operator[](size_t i) const
	{
		gloxAssert(i < size(), "Span access out of bounds");
		return *(from + i);
	}
	constexpr T& operator[](size_t i)
	{
		return const_cast<T&>(static_cast<const span<T>&>(*this)[i]);
	}

	template <int I>
	friend constexpr auto get(const span& sp)
	{
		if constexpr (I == 0)
			return sp.from;
		if constexpr (I == 1)
			return sp.to;
	}
};
// boiler plate for structure bindings
} // namespace glox

namespace std
{

template <typename T>
struct tuple_size<glox::span<T>> : ::std::integral_constant<size_t, 2>
{
};
template <typename T>
struct tuple_element<0, glox::span<T>>
{
	using type = T*;
};
template <typename T>
struct tuple_element<1, glox::span<T>>
{
	using type = T*;
};
} // namespace std
