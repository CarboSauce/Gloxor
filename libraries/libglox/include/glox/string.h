#pragma once
#ifndef USE_MODULES
#include "glox/types.hpp"
#endif

GLOX_EXPORT
namespace glox {
constexpr inline size_t strlen(char const* str)
{
    return __builtin_strlen(str);
}
constexpr inline void* memcpy(void* dst, void const* src, size_t size)
{
    return __builtin_memcpy(dst, src, size);
}
constexpr inline void* memset(void* dst, int byte, size_t size)
{
    return __builtin_memset(dst, byte, size);
}
constexpr inline void* memmove(void* dest, void const* src, size_t len)
{
    return __builtin_memmove(dest, src, len);
}
} // namespace glox
