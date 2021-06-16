#include "danger.hpp"
#include "glox/graphics.hpp"
#include "glox/algo.hpp"

extern glox::framebuffer con;
void glox::kernelPanic()
{

   auto width = con.width;
   auto height = con.height;
   auto beg = con.fbBeg;

   for (size_t i = 0; i < height; ++i)
   {
      for (size_t j = 0; j < width; ++j)
      {
         auto haha  = fixed_lerp<glox::rgb_t,1920>(0,255,j);
         auto haha2 = fixed_lerp<glox::rgb_t,1080>(0,255,i);
         beg[i*width + j] = 0xFF + (haha << 8) + (haha2 << 16);
      }
   }
}