export module gloxor.sched.scheduler;

import gloxor.sched.thread;
import arch;
import std;
import glox;
import gloxor.mutex;
import gloxor.lib.stack;
import gloxor.kinfo;

export namespace gx {

class Scheduler
{
    glox::deque<Thread*> ready_queue;
    Thread* current = nullptr;

    gx::IrqSpinLock lock;

public:
    void create_thread(glox::string name, gx::thread_func work, void* arg)
    {
        auto thread = new Thread(std::move(name), work, arg);

        glox::unique_lock guard { lock };
        ready_queue.push_back(thread);
    }

    void create_thread(glox::string name, StackBox box)
    {
        auto thread = new Thread(std::move(name), std::move(box));

        glox::unique_lock guard { lock };
        ready_queue.push_back(thread);
    }

    void yield()
    {
        Thread* prev;
        Thread* next;

        {
            glox::unique_lock guard { lock };

            if (ready_queue.empty())
                return;

            prev = current;

            if (prev != nullptr)
                ready_queue.push_back(prev);

            next = ready_queue.front();
            ready_queue.pop_front();

            current = next;
        }

        if (prev != next && prev != nullptr) {
            arch::switch_task(&prev->saved_sp(), &next->saved_sp());
        }
    }
};

inline Scheduler globalSched;

void init_sched()
{
    globalSched.create_thread(
        "boot",
        StackBox {
            (void*)gx::machineInfo.mainStack,
            (void*)gx::machineInfo.mainStack,
        }
    );
}

} // namespace gx
