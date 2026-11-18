export module gloxor.sched.thread;
import std;
import std.compat;
import glox;
import gloxor;
import arch;
import gloxor.memory.pmm;

export namespace gx {

using thread_func = void (*)(void*);

class Thread
{
    glox::string name;
    uintptr_t stackBase;
    uintptr_t stackTop;

public:
    Thread(glox::string name, thread_func func, void* arg)
        : name(std::move(name))
    {
        stackBase = (uintptr_t)gx::page_alloc(2);
        stackTop = arch::setup_stack(stackBase, func, arg);
    }

    ~Thread()
    {
        gx::page_dealloc((void*)stackBase, 2);
    }

    Thread(glox::string name)
        : name(std::move(name))
    {
    }

    uintptr_t stack_top()
    {
        return stackTop;
    }
};
} // namespace gx
