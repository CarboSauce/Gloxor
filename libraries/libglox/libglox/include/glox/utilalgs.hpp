#pragma once
#include <concepts>
#include <glox/string.hpp>
#include <utility>
// temporary


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
	}

	template <typename iterIn, typename iterOut>
	constexpr auto copyBackwards(iterIn first, iterIn last, iterOut d_last)
	{
		while (first != last)
		{
			*(--d_last) = *(--last);
		}
	}
	template <typename iterIn, typename iterOut>
	constexpr auto copyOverlapped(iterIn src, iterIn srcLast, iterOut dest)
	{
		if (dest < src)
		{
			copy(src,srcLast,dest);
		}
		else 
		{
			copyBackwards(src,srcLast,dest+(srcLast-src));
		}
	}

	template <typename T, typename... Args>
	T* construct(T* at, Args&&... args)
	{
		return ::new (const_cast<void*>(static_cast<const volatile void*>(at))) T(std::forward<Args>(args)...);
	}

} // namespace glox
