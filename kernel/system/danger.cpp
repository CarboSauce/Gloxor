module gloxor.system.danger;
import arch;
import gloxor.system.logging;
static inline void draw_panic()
{
    gx::println("Kernel panic");
}

extern "C" void abort()
{
    gx::kernel_panic();
}
namespace gx {
void kernel_panic()
{
    draw_panic();
    arch::halt_forever();
}
} // namespace gx
