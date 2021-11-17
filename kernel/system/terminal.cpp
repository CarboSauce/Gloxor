#include "terminal.hpp"
#include "drivers/e9.hpp"
#include "glox/string.hpp"
#include "gloxor/graphics.hpp"

using namespace glox;

const extern uint8_t fontBitmap[];
constexpr int scaleX = 1;
constexpr int scaleY = 1;
constexpr int fontWidth = 8 * scaleX;
constexpr int fontHeight = 16 * scaleY;
static glox::framebuffer con;
// TEEECHNICALLY we can storage optimize those
// to be able to set both of values (or maybe 4) in same operation,
// but it comes at a cost of making sure compiler wont break stuff,
// and LTO will most likely elide most of those, and overall notworth
static vec2<colorT> fgbg{0xFFFFFF, 0};
static vec2<u32> at{0, 0};
static u8 cursorShape = '_';

namespace glox::term
{

	inline void smallDelay()
	{
		for (volatile size_t i = 0; i < 3'000'000'000; i = i + 1)
		{
		}
	}

	inline void putPixel(int x, int y, colorT color)
	{
		con.fbBeg[y * con.pitch + x] = color;
	}

	inline void writeChar(char ch, const glox::vec2<u32>& at, const glox::vec2<colorT>& fgbg)
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

		/* 
        Magic code stolen from ted
   */

		for (u32 i = 0, i1 = 0, i2 = 0; i2 < fontHeight; ++i, ++i2, (i == scaleY) ? (i = 0, i1++) : i)
		{
			for (u32 j = 0, j1 = 0, j2 = 0; j2 < fontWidth; ++j, ++j2, (j == scaleX) ? (j = 0, j1++) : j)
			{
				auto fontBit = 0b10000000 >> (j1);
				auto fontMask = fontBitmap[index + i1];
				putPixel(at.x + j2, at.y + i2, fontMask & fontBit ? fgbg.x : fgbg.y);
			}
		}
	}
	inline void execNewline()
	{
		/**
		 * @todo this code is broken incase font height isnt divisible by fb height
		 * reason uknown
		 */
		auto lineOffset = con.pitch * fontHeight;
		auto endpos = con.fbBeg + con.pitch * (fontHeight + at.y);
		glox::copyOverlapped(con.fbBeg + lineOffset, endpos, con.fbBeg);
		at.x = 0;
		glox::setRange(con.fbBeg + con.pitch * at.y, con.fbEnd, fgbg.y);
	}

	inline void parseChar(char c)
	{
		// total hack, we scroll the buffer incase next write would overflow
		if (at.y + fontHeight > con.height)
		{
			at.y -= fontHeight;
			execNewline();
		}
		switch (c)
		{
			case '\n':
			{
				auto yoffset = at.y + fontHeight;
				if (con.fbBeg + con.pitch * yoffset >= con.fbEnd)
				{
					execNewline();
				}
				else
				{
					at.y = yoffset;
					at.x = 0;
				}
				break;
			}
			default:
				writeChar(c, at, fgbg);
				at.x += fontWidth;
				break;
		}
		// should we jump to next line?
		if (at.x + fontWidth >= con.pitch)
		{
			at.y += fontHeight;
			at.x = 0;
		}
	}

	void initTerm(colorT* begin, colorT* end, size_t pitch, size_t width, size_t height)
	{
		con =
			{
				.fbBeg = begin,
				.fbEnd = end,
				.width = width,
				.height = height,
				.pitch = pitch};
	}

	void clearScreen(colorT color)
	{
		//glox::drawRectangle(fbBeg,pitch,{0,0},{(colorT)width,(colorT)height},color);
		glox::setRange(con.fbBeg, con.fbEnd, color);
		at = {0, 0};
	}
	inline void printCursor()
	{
		/* 	auto temp = at;
		temp.x+=1;
		writeChar(cursorShape,temp,{0xffc0cb,fgbg.y}); */
	}
	inline void eraseCursor()
	{
	}
	void writeStr(const char* str, size_t size)
	{
		eraseCursor();
		for (size_t s = 0; s != size; ++s)
		{
			char c = str[s];
			parseChar(c);
		}
		printCursor();
	}

	void setFgColor(colorT fg)
	{
		fgbg.x = fg;
	}
	void setBgColor(colorT bg)
	{
		fgbg.y = bg;
	}
	void setCursorLook(u8 ascii)
	{
		cursorShape = ascii;
	}
} // namespace glox::term