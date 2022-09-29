#pragma once
#include "assert.hpp"
#include "type_traits"
#include "types.hpp"
#include "metaprog.hpp"

namespace glox
{
template<typename T>
void swap(T& l, T& r)
{
	auto tmp = RVALUE(l);
	l = RVALUE(r);
	r = RVALUE(tmp);
}
template<typename T, typename U = T>
T exchange(T& l, U&& r)
{
	auto tmp = RVALUE(l);
	l = r;
	return tmp;
}
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

template <typename T, typename E>
class result
{
	// static_assert(std::is_default_constructible<T>::value, "Result type needs to be default constructible");
	union
	{
		T val;
		E err;
	};
	bool isErr;

 public:
	constexpr result(T val) : val(RVALUE(val)), isErr(false) {}
	constexpr result(E err) : val(RVALUE(err)), isErr(true) {}
	T& unwrap()
	{
		gloxAssert(!isErr, "unwrap called with error set");
		return val;
	}

	constexpr operator bool()
	{
		return isErr;
	}
};

template <typename T>
using optional = result<T, bool>;

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
