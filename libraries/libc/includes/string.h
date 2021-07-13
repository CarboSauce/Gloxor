#ifndef SMOLLIB_STRING_H_
#define SMOLLIB_STRING_H_

#include <stddef.h>
#include "common/config.h"



BEGIN_EXTERN_C

void* memcpy(void *__restrict__ dst, const void *__restrict__ src, size_t size) SMOL_NOEXCEPT(true);
// {
//     return __builtin_memcpy(dst,src,size);
// }

void* memset(void* dst, int val, size_t size) SMOL_NOEXCEPT(true);
// {
//     return __builtin_memset(dst, val, size);
// }

void* memmove(void *__restrict__ dst,const void *__restrict__ src, size_t size) SMOL_NOEXCEPT(true);
// {
//     return __builtin_memmove(dst, src, size);
// }

int memcmp(const void* block1, const void* block2, size_t size) SMOL_NOEXCEPT(true);
// {
//     return __builtin_memcmp(block1,block2,size);
// }


// builtin strlen is hella broken for unknown reason

size_t strlen(const char* str) SMOL_NOEXCEPT(true);
// {
//     return __builtin_strlen(str);
// }

char* strcat(char* dst, const char* src) SMOL_NOEXCEPT(true);
// {
//     return __builtin_strcat(dst,src);
// }

char* strncat(char* dst, const char* src, size_t size) SMOL_NOEXCEPT(true);
// {
//     return __builtin_strncat(dst,src,size);
// }

char* strcpy(char* dst, const char* src) SMOL_NOEXCEPT(true);
// {
//     return __builtin_strcpy(dst,src);
// }

char* strncpy(char* dst, const char* src,size_t size) SMOL_NOEXCEPT(true);
// {
//     return __builtin_strncpy(dst,src,size);
// }

int strcmp(char* dst, const char* src) SMOL_NOEXCEPT(true);
// {
//     return __builtin_strcmp(dst,src);
// }

int strncmp(char* dst, const char* src, size_t size) SMOL_NOEXCEPT(true);
// {
//     return __builtin_strncmp(dst,src,size);
// }

END_EXTERN_C

#endif