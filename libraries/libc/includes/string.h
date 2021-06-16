#ifndef SMOLLIB_STRING_H_
#define SMOLLIB_STRING_H_

#include <stddef.h>
#include "common/config.h"



BEGIN_EXTERN_C

SMOL_INLINE void* memcpy(void *__restrict__ dst, const void *__restrict__ src, size_t size) SMOL_NOEXCEPT(true)
{
    return __builtin_memcpy(dst,src,size);
}

SMOL_INLINE void* memset(void* dst, int val, size_t size) SMOL_NOEXCEPT(true)
{
    return __builtin_memset(dst, val, size);
}

SMOL_INLINE void* memmove(void *__restrict__ dst,const void *__restrict__ src, size_t size) SMOL_NOEXCEPT(true)
{
    return __builtin_memmove(dst, src, size);
}

SMOL_INLINE int memmcmp(const void* block1, const void* block2, size_t size) SMOL_NOEXCEPT(true)
{
    return __builtin_memcmp(block1,block2,size);
}

SMOL_INLINE size_t strlen(const char* str) SMOL_NOEXCEPT(true)
{
    return __builtin_strlen(str);
}

SMOL_INLINE char* strcat(char* dst, const char* src) SMOL_NOEXCEPT(true)
{
    return __builtin_strcat(dst,src);
}

SMOL_INLINE char* strncat(char* dst, const char* src, size_t size) SMOL_NOEXCEPT(true)
{
    return __builtin_strncat(dst,src,size);
}

SMOL_INLINE char* strcpy(char* dst, const char* src) SMOL_NOEXCEPT(true)
{
    return __builtin_strcpy(dst,src);
}

SMOL_INLINE char* strncpy(char* dst, const char* src,size_t size) SMOL_NOEXCEPT(true)
{
    return __builtin_strncpy(dst,src,size);
}

SMOL_INLINE int strcmp(char* dst, const char* src) SMOL_NOEXCEPT(true)
{
    return __builtin_strcmp(dst,src);
}

SMOL_INLINE int strncmp(char* dst, const char* src, size_t size) SMOL_NOEXCEPT(true)
{
    return __builtin_strncmp(dst,src,size);
}

END_EXTERN_C

#endif