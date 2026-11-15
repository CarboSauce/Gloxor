export module arch_asm.idt;
import std.compat;

export {
constexpr auto IDT_INTERRUPTGATE = 0b10001110;
constexpr auto IDT_CALLGATE = 0b10001100;
constexpr auto IDT_TRAPGATE = 0b10001111;

struct InterruptFrame
{
    size_t ip;
    size_t cs;
    size_t flags;
    size_t sp;
    size_t ss;
};

namespace arch {
    struct [[gnu::packed]] InterruptRegs
    {
        uint64_t r15;
        uint64_t r14;
        uint64_t r13;
        uint64_t r12;
        uint64_t r11;
        uint64_t r10;
        uint64_t r9;
        uint64_t r8;

        uint64_t rbp;
        uint64_t rdi;
        uint64_t rsi;
        uint64_t rdx;
        uint64_t rcx;
        uint64_t rbx;
        uint64_t rax;

        uint64_t errorCode;

        uint64_t rip;
        uint64_t cs;
        uint64_t rflags;

        // Present only when the CPU switches stacks
        // (privilege-level change or IST).
        uint64_t rsp;
        uint64_t ss;
    };
} // namespace arch

struct [[gnu::packed]] Idt
{
    uint16_t offset_1; // offset bits 0..15
    uint16_t selector; // a code segment selector in GDT or LDT
    uint8_t
        ist; // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
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
    constexpr void register_handler(
        uint64_t offset,
        uint16_t selector,
        uint8_t ist,
        uint8_t type_attr
    )
    {
        this->selector = selector;
        this->type_attr = type_attr;
        this->ist = ist;
        set_offset(offset);
    }
};

struct [[gnu::packed]] IdtPointer
{
    uint16_t size;
    Idt* base;
};

inline void load_idt(const IdtPointer& ptr)
{
    asm("lidt %0" : : "m"(ptr));
}

inline IdtPointer get_idt()
{
    IdtPointer ptr;
    asm("sidt %0" : "=m"(ptr));
    return ptr;
}
}
