module gloxor.system.danger;
import arch;
import gloxor.system.logging;
static inline void draw_panic()
{
    gx::outStream, "\r\nKernel Panic moment";
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
