#pragma once
#include "assert.hpp"
#include "type_traits"
//#include <utility>
#include "types.hpp"

#define RVALUE(...) static_cast<std::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)
#define FORWARD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace glox
{

	template <typename T, typename U>
	struct pair
	{
		T first;
		U second;
      constexpr friend auto operator<=>(const pair&,const pair&) = default;
	};

	template <typename T>
	struct vec2
	{
		T x;
		T y;
      constexpr friend auto operator<=>(const vec2&,const vec2&) = default;
	};

   template<typename T>
   class span
   {
      T* from,*to;
      public:
      using iterator = T*;
		constexpr span(T* from, T* to) : from(from), to(to) 
      {
         gloxAssert(this->from < this->to,"Span shouldn't have negative range!");
      }
      constexpr iterator begin() const {return from;}
      constexpr iterator end() const {return to;}
      constexpr size_t size() const {return to-from;}
      constexpr const T& operator[](size_t i) const
      {
         gloxAssert(i >= 0 && i<size(),"Span access out of bounds");
         return *(from+i);
      }
      constexpr T& operator[](size_t i)
      {
         return const_cast<T&>(static_cast<const T&>(*this)[i]);
      }
   };


	template <typename T, typename E>
	class result
	{
		static_assert(std::is_default_constructible<T>::value, "Result type needs default constructible");
      T val;
		E err;
   public:
      constexpr result(T val, E err) : val(RVALUE(val)), err(RVALUE(err)) {}
		T& unwrap()
		{
			gloxAssert(!err, "unwrap called with error set");
			return val;
		}

		constexpr operator bool()
		{
			return static_cast<bool>(err);
		}
	};

	template <typename T>
	using optional = result<T, bool>;

} // namespace glox
