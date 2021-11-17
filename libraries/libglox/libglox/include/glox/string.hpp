#pragma once
/* 
#if __STDC_HOSTED__
   #include <cstring>
   #define GLOX_STRLEN(x) std::strlen(x)
#else
   // In freestanding we assume that user implements string.h
   #include <string.h>
   #define GLOX_STRLEN(x) strlen(x)
#endif
 */
#include "glox/types.hpp"

namespace glox
{
   inline size_t strlen(const char* str)
   { return __builtin_strlen(str);}
   inline void* memcpy(void* dst, const void* src, size_t size)
   { return __builtin_memcpy(dst,src,size);}
   inline void* memset(void* dst, int byte, size_t size )
   { return __builtin_memset(dst,byte,size);}
   inline void* memmove(void* dest, const void* src, size_t len)
   { return __builtin_memmove(dest,src,len);}
}

using glox::strlen;
using glox::memcpy;
using glox::memset;
using glox::memmove;