#pragma once
#include <concepts>
#include <glox/string.hpp>
#include <glox/metaprog.hpp>
#include "detail/moveutils.hpp"

namespace glox
{

template <typename iter, typename T>
constexpr iter find(iter a, iter b, const T& val)
{
	for (; a != b; ++a)
		if (*a == val)
			return a;
	return b;
}
template <typename iter, typename Cb>
constexpr iter find_if(iter a, iter b, Cb fn)
{
	for (; a != b; ++a)
		if (fn(*a))
			return a;
	return b;
}
template <typename iter>
constexpr void reverse(iter a, iter b)
{
	while ((a != b) && (a != --b))
	{
		glox::swap(*a++, *b);
	}
}

template <typename iter, typename T>
constexpr void set_range(iter beg, iter end, const T& val)
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
constexpr auto copy_backwards(iterIn first, iterIn last, iterOut d_last)
{
	while (first != last)
	{
		*(--d_last) = *(--last);
	}
}
template <typename iterIn, typename iterOut>
constexpr auto copy_overlapped(iterIn src, iterIn srcLast, iterOut dest)
{
	if (dest < src)
	{
		copy(src, srcLast, dest);
	}
	else
	{
		copy_backwards(src, srcLast, dest + (srcLast - src));
	}
}

template <typename T, typename... Args>
T* construct(T* at, Args&&... args)
{
	return ::new (const_cast<void*>(static_cast<const volatile void*>(at))) T(FWD(args)...);
}

} // namespace glox
