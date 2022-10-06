#ifdef LIBGLOX_FREESTANDING
#ifndef LIBGLOX_OPERATOR_NEW
#define LIBGLOX_OPERATOR_NEW
[[gnu::always_inline, nodiscard]] inline void* operator new(size_t, void* p) noexcept { return p; }
// dont use placement new array, broken by design
//[[gnu::always_inline, nodiscard]] inline void* operator new[](size_t, void* p) noexcept { return p; }
#else
#include <new>
#endif
#endif
