export module arch.sched;
import std.compat;

namespace arch {
extern "C" [[gnu::naked]]
void trampoline()
{
    asm volatile( //
        "movq %r13, %rdi\n"
        "call *%r12\n"
        "ud2\n"
    );
}

export void* setup_stack(void* stack_top, void (*fn)(void*), void* arg)
{
    void* sp = stack_top;
    asm volatile(
        // Align stack_top down to 16 bytes
        "andq $-16, %[sp]\n"
        // Return address popped by
        "movq %[tramp],  -8(%[sp])\n"
        "movq $0,       -16(%[sp])\n" // rbp
        "movq $0,       -24(%[sp])\n" // rbx
        "movq %[fn],    -32(%[sp])\n" // r12
        "movq %[arg],   -40(%[sp])\n" // r13
        "movq $0,       -48(%[sp])\n" // r14
        "movq $0,       -56(%[sp])\n" // r15
        "subq $56, %[sp]\n" // Advance sp to bottom of forged frame
        : [sp] "+&r"(sp)
        : [fn] "r"(fn), [arg] "r"(arg), [tramp] "r"(trampoline)
        : "memory"
    );
    return sp;
}

export [[gnu::naked]]
void switch_task(void** from, void** to)
{
    asm volatile(
        // Save callee-saved registers onto the current stack
        "pushq %%rbp\n"
        "pushq %%rbx\n"
        "pushq %%r12\n"
        "pushq %%r13\n"
        "pushq %%r14\n"
        "pushq %%r15\n"

        "movq %%rsp, (%%rdi)\n"
        "movq (%%rsi), %%rsp\n"

        "popq %%r15\n"
        "popq %%r14\n"
        "popq %%r13\n"
        "popq %%r12\n"
        "popq %%rbx\n"
        "popq %%rbp\n"

        "ret\n" ::
            : "memory"
    );
}

export inline void run_on_stack(uintptr_t stack, void (*fn)(void*), void* arg)
{
    register uintptr_t rg asm("rdi") = (uintptr_t)arg;
    asm volatile(
        //
        "mov %0, %%rsp\n"
        "call *%1\n"
        "ud2"
        :
        : "r"(stack), "r"(fn), "r"(rg)
        : "memory"
    );
}
export inline void run_on_stack(uintptr_t stack, void (*fn)())
{
    asm volatile(
        //
        "mov %0, %%rsp\n"
        "call *%1\n"
        "ud2"
        :
        : "r"(stack), "r"(fn)
        : "memory"
    );
}
} // namespace arch
