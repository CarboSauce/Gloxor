#pragma once
#include "gloxor/types.hpp"
namespace arch
{
	constexpr u64 kernelHalfBase = 0xffffffff80000000;
	constexpr u64 pageSize = 0x1000;
	constexpr u64 physicalMemBase = 0xffff800000000000;
	using pagingT = u64;
} // namespace arch