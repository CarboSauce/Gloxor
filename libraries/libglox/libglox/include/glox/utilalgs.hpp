#pragma once
#include <utility>


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

} // namespace glox
