#pragma once
#include <utility>
namespace glox
{

   template<typename iter>
   void reverse(iter a, iter b)
   {
      while ((a != b) && (a != --b))
      {
         std::swap(*a++, *b);
      }
   }

   template<typename iter,typename T>
   void setRange(iter beg, iter end, const T& val)
   {
      for(;beg != end; ++beg)
      {
         *beg = val;
      }
   }

} // namespace glox
