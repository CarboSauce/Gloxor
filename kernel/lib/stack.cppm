
export module gloxor.lib.stack;

import gloxor.memory.buddy;
import glox;
import std;

export namespace gx {

class StackBox
{
    void* stackBase = nullptr;
    void* savedSp = nullptr;

public:
    StackBox(void* base, void* sp)
        : stackBase(base)
        , savedSp(sp)
    {
    }
    StackBox(StackBox const&) = delete;
    StackBox& operator=(StackBox const&) = delete;

    StackBox(StackBox&& other)
        : stackBase(glox::exchange(other.stackBase, nullptr))
        , savedSp(glox::exchange(other.savedSp, nullptr))
    {
    }

    StackBox& operator=(StackBox&& other)
    {
        if (this != &other) {
            if (stackBase != nullptr)
                gx::buddy_free_order(stackBase, 2);

            stackBase = glox::exchange(other.stackBase, nullptr);
            savedSp = glox::exchange(other.savedSp, nullptr);
        }

        return *this;
    }

    ~StackBox()
    {
        if (stackBase != nullptr)
            gx::buddy_free_order(stackBase, 1);
    }

    static glox::option<StackBox> create()
    {
        auto stack = gx::buddy_alloc_order(1);

        if (stack == nullptr)
            return { };

        auto* base = static_cast<void*>(stack);
        auto* top = static_cast<void*>(
            static_cast<std::byte*>(base) + (gx::buddyAllocator.PAGE_SIZE * 2)
        );

        return StackBox { base, top };
    }

    void* base() const
    {
        return stackBase;
    }

    void* top() const
    {
        return static_cast<void*>(
            static_cast<std::byte*>(stackBase)
            + (gx::buddyAllocator.PAGE_SIZE * 2)
        );
    }

    void*& saved_sp()
    {
        return savedSp;
    }
    void* const& saved_sp() const
    {
        return savedSp;
    }

    void set_saved_sp(void* sp)
    {
        savedSp = sp;
    }
};

} // namespace gx
