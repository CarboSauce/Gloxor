export module gloxor.kinfo;
import std;
import std.compat;
import glox;
import gloxor.types;
#include <glox/assert.hpp>

export namespace gx {
struct BootInfo
{
    enum class MemTypes
    {
        usable,
        reserved,
        acpiReclaim,
        acpiNvs,
        badMem,
        reclaimable,
        kernel,
        framebuffer
    };
    struct MemoryMap
    {
        paddr base;
        usize length;
        MemTypes type;
    };
    struct FbInfo
    {
        paddr begin;
        paddr end;
        usize pitch;
        usize height;
        usize width;
        u8 redMaskSize;
        u8 redMaskShift;
        u8 greenMaskSize;
        u8 greenMaskShift;
        u8 blueMaskSize;
        u8 blueMaskShift;
    };

    ::glox::span<MemoryMap> mmapEntries;
    FbInfo fbInfoEntry;
    uintptr_t mainStackTop;
    uintptr_t mainStackSize;
    ::glox::span<u8> kernelCode;
};
std::string_view memtype_to_str(BootInfo::MemTypes memtype)
{
    switch (memtype) {
    case BootInfo::MemTypes::usable:
        return "USABLE";
    case BootInfo::MemTypes::reserved:
        return "RESERVED";
    case BootInfo::MemTypes::acpiReclaim:
        return "ACPI RECLAIMABLE";
    case BootInfo::MemTypes::acpiNvs:
        return "ACPI NVS";
    case BootInfo::MemTypes::badMem:
        return "BAD MEMORY";
    case BootInfo::MemTypes::reclaimable:
        return "RECLAIMABLE";
    case BootInfo::MemTypes::kernel:
        return "KERNEL";
    case BootInfo::MemTypes::framebuffer:
        return "FRAMEBUFFER";
    }
    GLOX_UNREACHABLE();
    return "UNKNOWN";
}
inline gx::BootInfo machineInfo;
} // namespace gx
