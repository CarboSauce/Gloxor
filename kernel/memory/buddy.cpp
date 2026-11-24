module gloxor.memory.buddy;

import std.compat;
import glox;
import gloxor.types;
import gloxor.memory.pmm;
import gloxor.system.logging;
import arch;

namespace gx {

void init_buddy()
{
    gx::println("[buddy] init buddy");
    gx::boot_reclaim([](void* p, usize s) {
        auto phys = arch::get_real_address((vaddr)p);
        buddyAllocator.add_region(phys, s);
    });
}

void* buddy_alloc(usize size)
{
    auto phys = buddyAllocator.alloc_bytes(size);
    if (phys == 0) {
        return nullptr;
    }
    return (void*)arch::to_virt(phys);
}
void* buddy_alloc_order(usize order)
{
    auto phys = buddyAllocator.alloc_order(order);
    if (phys == 0) {
        return nullptr;
    }
    return (void*)arch::to_virt(phys);
}
void buddy_free(void* ptr, usize size)
{
    if (ptr == nullptr) {
        return;
    }
    auto phys = arch::get_real_address((vaddr)ptr);
    buddyAllocator.free_bytes(phys, size);
}
void buddy_free_order(void* ptr, usize order)
{
    if (ptr == nullptr) {
        return;
    }
    auto phys = arch::get_real_address((vaddr)ptr);
    buddyAllocator.free_order(phys, order);
}
} // namespace gx
