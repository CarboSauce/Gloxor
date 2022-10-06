#ifndef GLOX_TUPLE
#define GLOX_TUPLE
#include "glox/metaprog.hpp"
#include "glox/macros.hpp"
#include <compare>
#include <cstddef>
#include <type_traits>
/*
 * Based on:
 * https://www.youtube.com/watch?v=TyiiNVA1syk
 */

namespace glox
{
template<typename... T>
struct type_pack{};
template <std::size_t I, typename T>
struct tuple_leaf
{
	[[no_unique_address]] T val;
	T& get(std::integral_constant<std::size_t, I>) { return val; }
	constexpr auto operator<=>(const tuple_leaf&) const = default;
	static T type_identity(std::integral_constant<std::size_t, I>);
};
template <typename... T>
struct tuple : tuple<glox::make_index_sequence<sizeof...(T)>, T...>
{
	constexpr static std::size_t size = sizeof...(T);
};
template <std::size_t... Idx, typename... T>
struct tuple<glox::index_sequence<Idx...>, T...> : tuple_leaf<Idx, T>...
{
	using tuple_leaf<Idx, T>::get...;
	using tuple_leaf<Idx, T>::type_identity...;
	using types = type_pack<T...>;
	constexpr static std::size_t size = sizeof...(T);
	template <std::size_t I>
	[[nodiscard]] constexpr auto& get() { return get(std::integral_constant<std::size_t, I>{}); }
};

template <typename... T>
tuple(T...) -> tuple<std::remove_reference_t<T>...>;

template<std::size_t I,typename Tup>
[[nodiscard]] constexpr decltype(auto) get(Tup&& tpl)
{
	return FORWARD(tpl).template get<I>();
}
namespace detail
{
	template<typename Cb, typename Tup, std::size_t... I>
	constexpr decltype(auto) apply(Cb&& cb, Tup&& tup, glox::index_sequence<I...>)
	{
		return FORWARD(cb)(FORWARD(tup).template get<I>()...);
	}
	template <typename Cb, typename Tup, std::size_t... I>
	constexpr void for_each(Tup&& tup, Cb&& cb,glox::index_sequence<I...>)
	{
		((void)cb(FORWARD(tup).template get<I>()),...);
	}
}
template <typename Cb,typename Tup>
constexpr void for_each(Tup&& tup, Cb&& cb)
{
	detail::for_each(FORWARD(tup),FORWARD(cb),
			glox::make_index_sequence<std::remove_reference_t<Tup>::size>{});
}
template<typename Cb, typename Tup>
constexpr decltype(auto) apply(Cb&& cb, Tup&& tup)
{
	return detail::apply(FORWARD(cb), FORWARD(tup), 
			glox::make_index_sequence<std::remove_reference_t<Tup>::size>{});
}

template<typename... T>
[[nodiscard]] constexpr glox::tuple<T...> make_tuple(T&&... args)
{
	return {FORWARD(args)...};
}
}; // namespace glox
namespace std
{
template<typename... T>
struct tuple_size<glox::tuple<T...>> 
	: std::integral_constant<std::size_t, sizeof...(T)> {};
template<std::size_t I, typename... T>
struct tuple_element<I, glox::tuple<T...>>
{ 
	using type = decltype( 
		glox::tuple<T...>::type_identity(
			std::integral_constant<std::size_t,I>{}
			));
};
}
#endif
