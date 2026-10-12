#pragma once
#define readCr(which)                              \
    ({                                             \
        size_t val;                                \
        asm("mov %%cr" #which ", %0" : "=r"(val)); \
        val;                                       \
    })
#define writeCr(which, val) ({ asm("mov %0,%%cr" #which::"r"(val)); })
