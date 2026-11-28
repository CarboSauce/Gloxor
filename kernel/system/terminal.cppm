module;
#include "flanterm.h"
export module gloxor.system.terminal;
import std.compat;
import glox;
import gloxor.types;
import gloxor.graphics;
import gloxor.kinfo;

export namespace gx::term {
void init_term(gx::BootInfo::FbInfo const& fb);
void write_str(char const* str, size_t size);
void write_str(std::string_view str);
inline void write_str(char const* str)
{
    write_str(str, glox::strlen(str));
}
glox::span<u8> get_used_memory_range();
constexpr color_t rgb(u8 r, u8 g, u8 b)
{
    return r << 16 | g << 8 | b;
}
} // namespace gx::term
