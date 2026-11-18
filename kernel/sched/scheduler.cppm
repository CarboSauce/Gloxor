export module gloxor.sched.scheduler;
import gloxor.sched.thread;
import arch;
import std;
import glox;
import gloxor.mutex;

export namespace gx {
class Scheduler
{
    glox::deque<Thread> threads;
    gx::IrqSpinLock lock;
    Thread currentThread;

public:
    void create_thread(glox::string name, gx::thread_func work, void* arg)
    {
        glox::unique_lock _ { lock };
        threads.push_back(std::move(name), work, arg);
    }

    void run_next()
    {
        glox::unique_lock _ { lock };
        auto nextThread = threads.front();
        threads.pop_front();
        threads.push_back(nextThread);
        arch::switch_task(currentThread.stack_top(), nextThread.stack_top());
    }
};
} // namespace gx
