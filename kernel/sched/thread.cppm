export module gloxor.sched.thread;
import std;
import std.compat;
import glox;
import gloxor;
import arch;
import gloxor.memory.pmm;
import gloxor.lib;

export namespace gx {

using thread_func = void (*)(void*);

class Thread
{
    glox::string name_;
    StackBox stack;

public:
    Thread(glox::string name, thread_func func, void* arg)
        : name_(std::move(name))
        , stack(StackBox::create().val())
    {
        stack.set_saved_sp(arch::setup_stack(stack.top(), func, arg));
    }

    Thread(glox::string name, StackBox stack)
        : name_(std::move(name))
        , stack(std::move(stack))
    {
    }

    void* const& saved_sp() const
    {
        return stack.saved_sp();
    }
    void*& saved_sp()
    {
        return stack.saved_sp();
    }
    void set_saved_sp(void* sp)
    {
        stack.set_saved_sp(sp);
    }

    glox::string const& name()
    {
        return name_;
    }
};
} // namespace gx
