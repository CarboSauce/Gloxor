#include "allocator.hpp"
// for now not compiled, reconsider if we want operators new/delete
void *operator new(size_t size)
{
	return glox::alloc(size);
}

void *operator new[](size_t size)
{
	return glox::alloc(size);
}

void operator delete(void *p,size_t size)
{
	glox::free(p,size);
}

void operator delete[](void *p,size_t size)
{
	glox::free(p,size);
}