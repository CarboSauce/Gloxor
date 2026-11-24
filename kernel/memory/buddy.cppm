export module gloxor.memory.buddy;
import gloxor.types;
import glox;
import gloxor.memory.pfndb;

export namespace gx {
inline glox::buddy_allocator<gx::PfndbAccessor> buddyAllocator;
void init_buddy();

void* buddy_alloc(usize size);
void* buddy_alloc_order(usize order);
void buddy_free(void* ptr, usize size);
void buddy_free_order(void* ptr, usize order);

struct BuddyAllocator
{
    [[nodiscard]] static glox::alloc_handle<void> alloc(usize s)
    {
        if (s == 0) {
            return { nullptr, 0 };
        }
        return { buddy_alloc(s), buddyAllocator.PAGE_SIZE / s + 1 };
    }
    static void dealloc(void* p, usize s)
    {
        if (p == nullptr)
            return;
        buddy_free(p, s);
    }
};

} // namespace gx
