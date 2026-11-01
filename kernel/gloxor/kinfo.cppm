export module gloxor.kinfo;
import glox;
import gloxor.types;

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
    ::glox::span<u8> kernelCode;
};
inline gx::BootInfo machineInfo;
} // namespace gx
