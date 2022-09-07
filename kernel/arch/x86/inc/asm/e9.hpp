#pragma once
#include "asm/asmstubs.hpp"
#include "gloxor/mutex.hpp"
#include "gloxor/types.hpp"

inline void print_e9(const char* str)
{
	glox::scoped_lock<gx::IrqLock> _;
	while (char c = *str++)
	{
		outb(0xE9, c);
	}
}

inline void print_e9(const char* str, size_t size)
{
	glox::scoped_lock<gx::IrqLock> _;
	for (size_t i = 0; i != size; ++i)
	{
		outb(0xE9, str[i]);
	}
}
