#pragma once

#include <cstdint>

struct [[gnu::packed]] gdt
{
	uint16_t limit;
	uint16_t base0_15;
	uint8_t base16_23;
	uint8_t flags;
	uint8_t granularity;
	uint8_t base24_31;
};

struct [[gnu::packed]] gdt_pointer
{
	uint16_t size;
	void* base;
};

inline void load_gdt(gdt_pointer& ptr)
{
	asm("lgdt %0"
		 :
		 : "m"(ptr));
}

inline void get_gdt(gdt_pointer& ptr)
{
	asm("sgdt %0"
		 : "=m"(ptr));
}