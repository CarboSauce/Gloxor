#include "gloxor/graphics.hpp"
#include <cstdint>

const extern uint8_t fontBitmap[];
constexpr int scaleX = 2;
constexpr int scaleY = 2;
constexpr int fontWidth = 8 * scaleX;
constexpr int fontHeight = 14 * scaleY;

namespace glox
{

   void framebuffer::putPixel(int x, int y, rgb_t color)
   {
      fbBeg[y * pitch + x] = color;
   }

   void framebuffer::writeCharAt(char ch, int x, int y, rgb_t fg, rgb_t bg)
   {
      int index = ((uint8_t)ch /* - 32 */) * 16;
      /* 
    auto prescX = x*scale;
    auto prescY = y*scale;
     */

      /*     for (int i = 0; i < fontHeight; ++i)
    {
        for (int k = 0; k < fontWidth; ++k)
        {
            auto fontBit =  0b10000000 >> (k/scaleX);
            auto fontMask = fontBitmap[index + (i/scaleY)];
            putPixel(x+k, y+i , fontMask & fontBit ? fg : bg);
        }

    } */

      /* 
        Magic code stolen from ted
     */

      for (int i = 0, i1 = 0, i2 = 0; i2 < fontHeight; ++i, ++i2, (i == scaleY) ? (i = 0, i1++) : i)
      {
         for (int j = 0, j1 = 0, j2 = 0; j2 < fontWidth; ++j, ++j2, (j == scaleX) ? (j = 0, j1++) : j)
         {
            auto fontBit = 0b10000000 >> (j1);
            auto fontMask = fontBitmap[index + i1];
            putPixel(x + j2, y + i2, fontMask & fontBit ? fg : bg);
         }
      }
   }



   void framebuffer::writeString(const char* str)
   {
      char c;
      while ((c = *str++) != 0)
      {
         writeCharAt(c, curX, curY, print_color, 0x0);

         // todo: modfify to work correctly with pitch values
         if (size_t tempX; (tempX = curX + fontWidth) < this->pitch)
         {
            curX = tempX;
         }
         else
         {
            curY += fontHeight;
            curX = 0;
         }
      }
   }
   void framebuffer::writeString(const char* str,size_t size)
   {
      size_t s = 0;
      char c;
      while ((c = str[s++]),s != size)
      {
         writeCharAt(c, curX, curY, print_color, 0x0);

         // todo: modfify to work correctly with pitch values
         if (size_t tempX; (tempX = curX + fontWidth) < this->pitch)
         {
            curX = tempX;
         }
         else
         {
            curY += fontHeight;
            curX = 0;
         }
      }
   }

   void framebuffer::cls(rgb_t color)
   {
      //glox::drawRectangle(fbBeg,pitch,{0,0},{(rgb_t)width,(rgb_t)height},color);
      glox::setRange(fbBeg,fbEnd,color);
   }

} // namespace glox