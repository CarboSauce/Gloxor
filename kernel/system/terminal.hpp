#pragma once
#include "glox/string.hpp"
#include "glox/util.hpp"
#include "gloxor/graphics.hpp"
#include "gloxor/types.hpp"

namespace glox::term
{
void initTerm(colorT* begin, colorT* end, size_t pitch, size_t width, size_t height);
void writeStr(const char* str, size_t size);
inline void writeStr(const char* str) { writeStr(str, strlen(str)); }
void setFgColor(colorT);
void setBgColor(colorT);
void clearScreen(colorT);
/**
 * @brief Sets the cursor look to the parameter, value of 0 implies no cursor
 */
void setCursorLook(u8 ascii);
glox::span<u8> getUsedMemoryRange();
constexpr colorT rgb(u8 r, u8 g, u8 b)
{
	return r << 16 | g << 8 | b;
}
} // namespace glox::term