export module arch_asm:pit8042;
import gloxor : types;

namespace gx::pit {
// 0xE8FF: Thats about 20Hz, its quite big
// 2386 is 500Hz, pretty good for nonVM since actuall hardware can do it
constexpr u16 timerFreq = 23860; // 4772;
constexpr u64 baseFreq = 1193182;
void set_divisor(u16 divisor);
} // namespace gx::pit
