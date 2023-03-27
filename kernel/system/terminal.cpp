#include "terminal.hpp"
#include "glox/string.hpp"
#include "gloxor/graphics.hpp"

using namespace gx;

const extern uint8_t fontBitmap[];
constexpr int scaleX = 1;
constexpr int scaleY = 1;
constexpr int fontWidth = 8 * scaleX;
constexpr int fontHeight = 16 * scaleY;
static gx::Framebuffer con;
// TEEECHNICALLY we can storage optimize those
// to be able to set both of values (or maybe 4) in same operation,
// but it comes at a cost of making sure compiler wont break stuff,
// and LTO will most likely elide most of those, and overall notworth
static glox::vec2<color_t> fgbg { 0xFFFFFF, 0 };
static glox::vec2<u32> at { 0, 0 };
static u8 cursorShape = '_';
namespace gx::term {
glox::span<u8> get_used_memory_range()
{
	return { (u8*)con.fbBeg, (u8*)con.fbEnd };
}
inline void put_pixel(int x, int y, color_t color)
{
	const_cast<volatile color_t*>(con.fbBeg)[y * con.pitch + x] = color;
}

inline void write_char(char ch, glox::vec2<u32> at, glox::vec2<color_t> fgbg)
{
	int index = ((uint8_t)ch /* - 32 */) * 16;
	/*
 auto prescX = x*scale;
 auto prescY = y*scale;
  */

	/* 	    for (int i = 0; i < fontHeight; ++i)
 {
	  for (int k = 0; k < fontWidth; ++k)
	  {
	        auto fontBit =  0b10000000 >> (k/scaleX);
	        auto fontMask = fontBitmap[index + (i/scaleY)];
	        putPixel(at.x+k, at.y+i , fontMask & fontBit ? fgbg.x : fgbg.y);
	  }

 } */

	/*Magic code stolen from ted uses no division*/

	for (u32 i = 0, i1 = 0, i2 = 0; i2 < fontHeight; ++i, ++i2, (i == scaleY) ? (i = 0, i1++) : i) {
		for (u32 j = 0, j1 = 0, j2 = 0; j2 < fontWidth; ++j, ++j2, (j == scaleX) ? (j = 0, j1++) : j) {
			auto fontBit = 0b10000000 >> (j1);
			auto fontMask = fontBitmap[index + i1];
			put_pixel(at.x + j2, at.y + i2, fontMask & fontBit ? fgbg.x : fgbg.y);
		}
	}
}
inline void exec_newline()
{
	/**
	 * @todo this code is broken incase font height isnt divisible by fb height
	 * reason uknown
	 */
	auto lineOffset = con.pitch * fontHeight;
	auto endpos = con.fbBeg + con.pitch * (fontHeight + at.y);
	glox::copy_overlapped(con.fbBeg + lineOffset, endpos, con.fbBeg);
	at.x = 0;
	glox::set_range(con.fbBeg + con.pitch * at.y, con.fbEnd, fgbg.y);
}

inline void parse_char(char c)
{
	// total hack, we scroll the buffer incase next write would overflow
	if (at.y + fontHeight > con.height) {
		at.y -= fontHeight;
		exec_newline();
	}
	switch (c) {
	case '\n': {
		auto yoffset = at.y + fontHeight;
		if (con.fbBeg + con.pitch * yoffset >= con.fbEnd) {
			exec_newline();
		} else {
			at.y = yoffset;
			at.x = 0;
		}
		break;
	}
	default:
		write_char(c, at, fgbg);
		at.x += fontWidth;
		break;
	}
	// should we jump to next line?
	if (at.x + fontWidth >= con.pitch) {
		at.y += fontHeight;
		at.x = 0;
	}
}

void init_term(color_t* begin, color_t* end, size_t pitch, size_t width, size_t height)
{
	con = {
		.fbBeg = begin,
		.fbEnd = end,
		.width = width,
		.height = height,
		.pitch = pitch
	};
}

void clear_screen(color_t color)
{
	// gx::drawRectangle(fbBeg,pitch,{0,0},{(colorT)width,(colorT)height},color);
	glox::set_range(con.fbBeg, con.fbEnd, color);
	at = { 0, 0 };
}
inline void print_cursor()
{
	/* 	auto temp = at;
	temp.x+=1;
	writeChar(cursorShape,temp,{0xffc0cb,fgbg.y}); */
}
inline void erase_cursor()
{
}
void write_str(const char* str, size_t size)
{
	erase_cursor();
	for (size_t s = 0; s != size; ++s) {
		char c = str[s];
		parse_char(c);
	}
	print_cursor();
}

void set_fg_color(color_t fg)
{
	fgbg.x = fg;
}
void set_bg_color(color_t bg)
{
	fgbg.y = bg;
}
void set_cursor_look(u8 ascii)
{
	cursorShape = ascii;
}
} // namespace gx::term
