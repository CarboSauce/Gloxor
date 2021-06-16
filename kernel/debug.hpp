#pragma once
#include "asm/asmstubs.hpp"
#include "glox/types.hpp"
#include "arch/irq.hpp"


inline void  print_debug(const char* str)
{
    //auto isIrq = arch::saveIrq();
    
    while (char c = *str++)
    {

        outb(0xE9, c);

    }

    //arch::restoreIrq(isIrq);
}

#ifndef NDEBUG
    #define DEBUG_PRINT(str) print_debug(str)
#else 
    #define DEBUG_PRINT(str)
#endif
