#pragma once

#include "glox/utilalgs.hpp"
#include <cstdint>
#include <type_traits>

namespace glox
{
   template <typename callback_t>
   void log_pointer (const callback_t& callback, const void* addr)
   {
      uintptr_t address = (uintptr_t)addr;
      constexpr auto size_of_ptrm1 = sizeof (address) * 8 - 4;
      uintptr_t moving_filter = static_cast<uintptr_t> (0xF) << size_of_ptrm1;
      char buffer[30]{};
      int i{ 0 };
      callback ("0x");
      while (moving_filter != 0)
      {
         auto index = moving_filter & address;
         index >>= size_of_ptrm1 - 4 * i;
         moving_filter /= 16;
         char hexDigit = "0123456789ABCDEF"[index];
         buffer[i++] = hexDigit;
      }
      buffer[i] = '\0';
      callback (buffer);
   }

   template <typename callback_t, typename T>
   void log_integer (callback_t callback, T value)
   {

      static_assert (std::is_integral<T>::value && std::is_unsigned<T>::value, "Provided non unsigned integer");

      char buffer[30]{};

      int i{ 0 };

      while (value > 0)
      {
         auto digit = value % 10;
         value /= 10;
         buffer[i++] = '0' + digit;
      }

      glox::reverse (buffer, buffer + i);
      buffer[i] = '\0';
      callback (buffer);
   }

}; // namespace glox