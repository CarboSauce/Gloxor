export module gloxor.memory.alloc;
import gloxor.memory.pmm;
import std.compat;

export namespace gx {
struct PmmAllocator
{
    [[nodiscard]] static void* alloc(size_t s)
    {
        return gx::page_alloc(s / pmmChunkSize + 1);
    }
    static void dealloc(void* p, size_t s)
    {
        gx::page_dealloc(p, s / pmmChunkSize + 1);
    }
};

void* memalloc(size_t size);
void memdealloc(void* ptr, size_t size);

template <typename T>
T* alloc(size_t ele_count = 1)
{
    T* ptr = (T*)(memalloc(sizeof(T) * ele_count));
    if (ptr == nullptr)
        return nullptr;
    for (size_t i = 0; i < ele_count; ++i) {
        ::new (ptr + i) T();
    }
    return ptr;
}
template <typename T>
void dealloc(T* ptr, size_t ele_count)
{
    if (ptr == nullptr)
        return;
    for (size_t i = 0; i < ele_count; ++i) {
        ptr[i].~T();
    }
    memdealloc(ptr, sizeof(T) * ele_count);
}

} // namespace gx
