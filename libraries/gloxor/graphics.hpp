#pragma once 
#include "gloxor/types.hpp"
#include "glox/pair.hpp"
#include "glox/utilalgs.hpp"

namespace glox {
    
using rgb_t = uint32_t;



struct framebuffer
{
    rgb_t* fbBeg;
    rgb_t* fbEnd;
    size_t pitch;
    size_t width;
    size_t height;
    rgb_t print_color;
    size_t curX = 0;
    size_t curY = 0;
    void writeCharAt(char,int,int, rgb_t fg, rgb_t bg);
    void writeString(const char*);
    void cls(rgb_t);
    void putPixel(int x, int y, rgb_t color);
};




template<typename iter,typename T>
void drawRectangle(iter beg, size_t fbwidth,glox::vec2<T> pos, glox::vec2<T> size, T color)
{
    for (auto i = beg+pos.y; i < beg+size.y*fbwidth; i+=fbwidth)
    {
        glox::setRange(i, i+size.x,color);
    }
}

} // namespace glox
