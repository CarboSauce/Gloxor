#pragma once
#include <glox/util.hpp>
#include <gloxor/types.hpp>

namespace gx {
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
    };

    glox::span<MemoryMap> mmapEntries;
    FbInfo fbInfoEntry;
    glox::span<u8> kernelCode;
};
inline gx::BootInfo machineInfo;
} // namespace gx
