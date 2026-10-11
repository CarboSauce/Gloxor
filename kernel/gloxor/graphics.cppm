export module gloxor:graphics;
import glox;
import gloxor : types;

export namespace gx {
using color_t = u32;

struct Framebuffer
{
    color_t* fbBeg;
    color_t* fbEnd;
    size_t width;
    size_t height;
    size_t pitch;
};

template <typename iter, typename T>
void draw_rectangle(
    iter beg,
    size_t fbwidth,
    ::glox::vec2<T> pos,
    ::glox::vec2<T> size,
    T color
)
{
    for (auto i = beg + pos.y; i < beg + size.y * fbwidth; i += fbwidth) {
        ::glox::set_range(i, i + size.x, color);
    }
}
} // namespace gx
