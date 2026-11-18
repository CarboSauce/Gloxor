export module gloxor.sched.scheduler;
import gloxor.sched.thread;
import glox;

export namespace gx {
class Scheduler
{
    glox::deque<Thread> threads;

public:
    void create_thread(glox::string name, glox::function<void()> work)
    {
        threads.push_back(name, work);
    }
};
} // namespace gx
