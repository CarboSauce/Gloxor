#ifndef CARBOS_SHELL_HPP_
#define CARBOS_SHELL_HPP_
#include <cstdint>
#include <cstddef>

namespace sane {
    
using rgb_t = uint32_t;

struct shell
{
    rgb_t* fbBeg;
    rgb_t* fbEnd;
    size_t width;
    size_t height;
    rgb_t print_color;
    void writeCharAt(char,int,int, rgb_t fg, rgb_t bg);
    void writeString(const char*);
    void cls(rgb_t);
    void putPixel(int x, int y, rgb_t color);
};

} // namespace sane

#endif