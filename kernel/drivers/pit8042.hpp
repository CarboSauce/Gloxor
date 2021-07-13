#pragma once
#include "gloxor/types.hpp"



namespace glox::pit
{
   // Thats about 20Hz, its quite big
   constexpr u16 timerFreq = 0xE8FF;
   constexpr u64 baseFreq = 1193182;
   void setDivisor(u16 divisor);
}