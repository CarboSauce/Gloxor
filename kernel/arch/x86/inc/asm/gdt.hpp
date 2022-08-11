#pragma once

#include <cstdint>

struct [[gnu::packed]] Gdt
{
	uint16_t limit;
	uint16_t base0_15;
	uint8_t base16_23;
	uint8_t flags;
	uint8_t granularity;
	uint8_t base24_31;
};

struct [[gnu::packed]] GdtPointer
{
	uint16_t size;
	void* base;
};

inline void load_gdt(GdtPointer& ptr)
{
	asm("lgdt %0"
		 :
		 : "m"(ptr));
}

inline void get_gdt(GdtPointer& ptr)
{
	asm("sgdt %0"
		 : "=m"(ptr));
}