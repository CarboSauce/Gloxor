#pragma once
#include "gloxor/types.hpp"
namespace arch
{
   constexpr u64 higherHalf = 0xffffffff80000000;
   constexpr auto pageSize = 0x1000;
   using pagingT = u64;
}