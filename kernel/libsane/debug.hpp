#ifndef CARBOS_DEBUG_KERNEL_HPP
#define CARBOS_DEBUG_KERNEL_HPP

#include "asmstubs.h"

inline void print_debug(const char* str)
{
    while (char c = *str++)
    {
        outb(0xE9, c);
    }
}

#ifndef NDEBUG
    #define DEBUG_PRINT(str) print_debug(str)
#else 
    #define DEBUG_PRINT(str)
#endif

#endif