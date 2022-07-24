#pragma once
#include "asm/asmstubs.hpp"
#include "gloxor/mutex.hpp"
#include "gloxor/types.hpp"

inline void printE9(const char* str)
{
	glox::scopedLock<glox::irqMutex> _;
	while (char c = *str++)
	{
		outb(0xE9, c);
	}
}

inline void printE9(const char* str, size_t size)
{
	glox::scopedLock<glox::irqMutex> _;
	for (size_t i = 0; i != size; ++i)
	{
		outb(0xE9, str[i]);
	}
}
