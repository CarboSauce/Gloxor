export module gloxor.sched.thread;
import std;
import std.compat;
import glox;
import gloxor;
import arch;

export namespace gx {
class Thread
{
    glox::string name;
    glox::function<void()> func;
    glox::box<u8[]> stack;

public:
    Thread(glox::string name, glox::function<void()> func)
        : name(std::move(name))
        , func(std::move(func))
        , stack { glox::box<u8[]>::create(8192) }
    {
        arch::setup_stack(
            (uintptr_t)stack.get_ptr(),
            [](void* arg) {
                auto func = static_cast<glox::function<void()>*>(arg);
                return (*func)();
            },
            &func
        );
    }
};
} // namespace gx
