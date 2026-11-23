module gloxor.memory.buddy;

import glox;
import gloxor.types;

namespace {

glox::buddy_allocator<>

}

namespace gx {

void buddy_init();

void* buddy_alloc(usize size);
void* buddy_alloc_order(usize order);
void* buddy_free(void* ptr, usize size);
void* buddy_free_order(void* ptr, usize order);
} // namespace gx
