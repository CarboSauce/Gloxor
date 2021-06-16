#pragma once

namespace glox
{

   template <typename T, typename U>
   struct pair
   {
      T first;
      U second;
   };

    template <typename T, typename U>
    struct vec2 
    {
        T x;
        U y;
    };
} // namespace glox
