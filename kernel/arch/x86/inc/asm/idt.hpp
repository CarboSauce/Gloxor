#pragma once
#include <cstddef>
#include <cstdint>
#define IDT_INTERRUPTGATE 0b10001110
#define IDT_CALLGATE 0b10001100
#define IDT_TRAPGATE 0b10001111

struct InterruptFrame {
	size_t ip;
	size_t cs;
	size_t flags;
	size_t sp;
	size_t ss;
};

struct [[gnu::packed]] Idt {
	uint16_t offset_1; // offset bits 0..15
	uint16_t selector; // a code segment selector in GDT or LDT
	uint8_t ist;       // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
	uint8_t type_attr; // type and attributes
	uint16_t offset_2; // offset bits 16..31
#ifdef __amd64
	uint32_t offset_3; // offset bits 32..63
	uint32_t zero = 0; // reserved
#endif

	constexpr void set_offset(uint64_t offset)
	{
		offset_1 = offset & 0xFFFF;
		offset_2 = (offset & 0xFFFF0000) >> 16;
		offset_3 = (offset & 0xFFFFFFFF00000000) >> 32;
	}
	constexpr void register_handler(uint64_t offset, uint16_t selector, uint8_t ist, uint8_t type_attr)
	{
		this->selector = selector;
		this->type_attr = type_attr;
		this->ist = ist;
		set_offset(offset);
	}
};

struct [[gnu::packed]] IdtPointer {
	uint16_t size;
	Idt* base;
};

inline void load_idt(const IdtPointer& ptr)
{
	asm("lidt %0"
	    :
	    : "m"(ptr));
}

inline IdtPointer get_idt()
{
	IdtPointer ptr;
	asm("sidt %0"
	    : "=m"(ptr));
	return ptr;
}
