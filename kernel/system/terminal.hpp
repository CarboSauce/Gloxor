#pragma once
#include "glox/string.hpp"
#include "glox/util.hpp"
#include "gloxor/graphics.hpp"
#include "gloxor/types.hpp"

namespace glox::term
{
void init_term(color_t* begin, color_t* end, size_t pitch, size_t width, size_t height);
void write_str(const char* str, size_t size);
inline void write_str(const char* str) { write_str(str, strlen(str)); }
void set_fg_color(color_t);
void set_bg_color(color_t);
void clear_screen(color_t);
/**
 * @brief Sets the cursor look to the parameter, value of 0 implies no cursor
 */
void set_cursor_look(u8 ascii);
glox::span<u8> get_used_memory_range();
constexpr color_t rgb(u8 r, u8 g, u8 b)
{
	return r << 16 | g << 8 | b;
}
} // namespace glox::term