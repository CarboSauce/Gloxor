#include "alloc.hpp"

void *operator new(size_t size)
{
	return glox::memalloc(size);
}

void *operator new[](size_t size)
{
	return glox::memalloc(size);
}

void operator delete(void *p,size_t size)
{
	glox::memdealloc(p,size);
}

void operator delete[](void *p,size_t size)
{
	glox::memdealloc(p,size);
}

namespace glox
{
	
}