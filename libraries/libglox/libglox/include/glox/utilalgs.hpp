#pragma once
#include <concepts>
#include <glox/string.hpp>
#include <utility>
// temporary
#include <bits/stl_construct.h>
#include <bits/stl_uninitialized.h> 
namespace glox
{

	template <typename iter>
	constexpr void reverse(iter a, iter b)
	{
		while ((a != b) && (a != --b))
		{
			std::swap(*a++, *b);
		}
	}

	template <typename iter, typename T>
	constexpr void setRange(iter beg, iter end, const T& val)
	{
		for (; beg != end; ++beg)
		{
			*beg = val;
		}
	}

	template <typename iterIn, typename iterOut>
	constexpr auto copy(iterIn in, iterIn inEnd, iterOut out)
	{
		while (in != inEnd)
		{
			*out++ = *in++;
		}
		return out;
	}

	template<typename T, typename... Args>
	T* construct(T* at, Args&&... args)
	{
		return ::new(const_cast<void*>(static_cast<const volatile void*>(at))) T(std::forward<Args>(args)...);
	}


} // namespace glox
