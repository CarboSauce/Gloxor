#pragma once
#include "glox/types.hpp"
namespace glox
{
   template<typename T,typename U>
   T bitmask(T a, U b)
   {
      return a & b;
   }

   template<typename T>
   T setbit(T a, unsigned int b)
   {
      return a | (1 << b);
   }

   template<typename T>
   T checkbit(T a, unsigned int b)
   {
      return a & (1 << b);
   }
   
}