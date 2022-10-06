#pragma once
#include "glox/types.hpp"
#include "detail/opnew.hpp"
namespace glox
{
template <typename T>
void uninit_def_construct(T first, T last)
{
	for (; first != last; first++)
		::new (first) T;
}
template <typename T>
void uninit_val_construct(T first, T last)
{
	for (; first != last; first++)
		::new (first) T();
}
template <typename T, typename Allocator>
T* alloc(Allocator a, size_t size = 1)
{
	T* ptr = (T*)(a.alloc(sizeof(T) * size));
	if (ptr == nullptr)
		return nullptr;
	for (size_t i = 0; i < size; ++i)
	{
		::new (ptr + i) T();
	}
	return ptr;
}
template <typename T, typename Allocator>
void dealloc(Allocator a, T* ptr, size_t size = 1)
{
	if (ptr == nullptr)
		return;
	for (size_t i = 0; i < size; ++i)
	{
		ptr[i].~T();
	}
	a.dealloc(ptr, sizeof(T) * size);
}
} // namespace glox

#ifdef LIBGLOX_DEFAULT_ALLOCATOR_PATH
#include LIBGLOX_DEFAULT_ALLOCATOR_PATH
#else
#include <cstdlib>
namespace glox
{
struct default_allocator
{
	void* allocate(size_t s)
	{	
		return std::malloc(s); 
	}
	void deallocate(void* p, [[maybe_unused]] size_t s)
	{
		return std::free(p);
	}
	void* reallocate(void* p, [[maybe_unused]] size_t old_size, size_t new_size)
	{
		return std::realloc(p,new_size);
	}
};
}
#endif 

