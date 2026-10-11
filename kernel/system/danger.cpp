import arch;
import gloxor : system;
static inline void draw_panic()
{
    gx::outStream, "\nKernel Panic moment";
}

void gx::kernel_panic()
{
    draw_panic();
    arch::halt_forever();
}
