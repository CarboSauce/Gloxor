#pragma once
#include "glox/types.hpp"
[[nodiscard]] inline void* operator new(size_t, void* p) { return p;}
[[nodiscard]] inline void* operator new[](size_t, void* p) { return p;}

namespace glox
{
	template<typename T>
	void uninitDefConstruct(T first, T last)
	{
		for (;first != last; first++)
			::new (first) T;
	}
	template<typename T>
	void uninitValConstruct(T first, T last)
	{
		for (;first != last; first++)
			::new (first) T();
	}
}