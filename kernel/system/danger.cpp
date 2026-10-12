module gloxor.system.danger;
import arch;
import gloxor.system.logging;
static inline void draw_panic()
{
    gx::outStream, "\nKernel Panic moment";
}

namespace gx {
void kernel_panic()
{
    draw_panic();
    arch::halt_forever();
}
} // namespace gx
