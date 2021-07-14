#pragma once

namespace glox
{

   template <typename T, typename U>
   struct pair
   {
      T first;
      U second;
   };

    template <typename T>
    struct vec2 
    {
        T x;
        T y;
    };
} // namespace glox
