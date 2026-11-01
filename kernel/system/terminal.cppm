export module gloxor.system.terminal;
import std.compat;
import glox;
import gloxor.types;
import gloxor.graphics;
import gloxor.kinfo;
#include "flanterm.h"

export namespace gx::term {
void init_term(const gx::BootInfo::FbInfo& fb);
void write_str(const char* str, size_t size);
inline void write_str(const char* str)
{
    write_str(str, glox::strlen(str));
}
glox::span<u8> get_used_memory_range();
constexpr color_t rgb(u8 r, u8 g, u8 b)
{
    return r << 16 | g << 8 | b;
}
} // namespace gx::term
