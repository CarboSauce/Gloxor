#ifndef LIBGLOX_OPERATOR_NEW
#define LIBGLOX_OPERATOR_NEW
#if __STDC_HOSTED__==1
#include <new>
#else
[[gnu::always_inline, nodiscard]] inline void* operator new(size_t, void* p) noexcept { return p; }
// dont use placement new array, broken by design
//[[gnu::always_inline, nodiscard]] inline void* operator new[](size_t, void* p) noexcept { return p; }
#endif
#endif
