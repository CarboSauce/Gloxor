module;
#include "flanterm_backends/fb.h"
module gloxor.system.terminal;
import gloxor.graphics;
import gloxor.types;
import gloxor.memory;
import std.compat;
import glox;

using namespace gx;

extern "C++" const u8 fontBitmap[];

namespace gx::term {
static flanterm_context* flantermContext = nullptr;
glox::span<u8> get_used_memory_range()
{
    return {
        (u8*)machineInfo.fbInfoEntry.begin,
        (u8*)machineInfo.fbInfoEntry.end,
    };
}

void init_term(const gx::BootInfo::FbInfo& fb)
{
    flantermContext = flanterm_fb_init(
        NULL, //[](size_t s) { return gx::boot_alloc(s); },
        NULL, //[](void* p, size_t s) { return gx::boot_dealloc(p, s); },
        (uint32_t*)fb.begin,
        fb.width,
        fb.height,
        fb.pitch,
        fb.redMaskSize,
        fb.redMaskShift,
        fb.greenMaskSize,
        fb.greenMaskShift,
        fb.blueMaskSize,
        fb.blueMaskShift,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        (void*)&fontBitmap,
        8,
        16,
        1,
        0,
        0,
        0,
        0
    );
}
void write_str(const char* str, size_t size)
{
    if (flantermContext != nullptr)
        flanterm_write(flantermContext, str, size);
}
void write_str(std::string_view str)
{
    if (flantermContext != nullptr)
        flanterm_write(flantermContext, str.data(), str.length());
}
void clear_screen(color_t)
{
    if (flantermContext != nullptr)
        flanterm_write(flantermContext, "\e[2J", 4);
}
} // namespace gx::term
