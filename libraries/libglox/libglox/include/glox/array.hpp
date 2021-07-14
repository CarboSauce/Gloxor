#pragma once
#include "glox/types.hpp"

namespace glox
{
	template <typename T, size_t size>
	struct array
	{
		T data[size];
		T& operator[](size_t i){ return data[i];}
		const T& operator[](size_t i) const { return data[i];}
		explicit operator T*() {return data;}
		explicit operator const T*() const {return data;}
	};
} // namespace glox