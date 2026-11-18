export module arch_asm.msr;
import gloxor.types;
import std.compat;

export namespace arch::msr {
enum Msr : uint32_t
{
    IA32_PAT = 0x00000277, // Page Attribute Table
    IA32_EFER = 0xC0000080, // EFER register, the non model specific one
    IA32_FS_BASE = 0xC0000100, // FS Base register
    IA32_GS_BASE = 0xC0000101, // GS Base register, used for cpu local vars
    IA32_KGS_BASE = 0xC0000102, // Kernel GS Base
};

inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}
inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" ::"c"(msr), "a"(low), "d"(high));
}
inline void wrmsr(uint32_t msr, uint32_t low, uint32_t high)
{
    asm volatile("wrmsr" ::"c"(msr), "a"(low), "d"(high));
}
}; // namespace arch::msr
