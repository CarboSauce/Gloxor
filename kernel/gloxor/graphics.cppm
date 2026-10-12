export module gloxor.graphics;
import glox;
import gloxor.types;

export namespace gx {
using color_t = u32;

struct Framebuffer
{
    color_t* fbBeg;
    color_t* fbEnd;
    usize width;
    usize height;
    usize pitch;
};

template <typename iter, typename T>
void draw_rectangle(
    iter beg,
    usize fbwidth,
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
