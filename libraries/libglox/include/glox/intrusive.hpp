#pragma once

#include "glox/macros.hpp"
#ifndef USE_MODULES
#include <cstddef>
#endif

namespace glox {
GLOX_BEGIN_EXPORT
template <typename Base, typename Member>
GLOX_ALWAYS_INLINE inline std::ptrdiff_t offset_of(const Member Base::* ptr)
{
    alignas(Base) char ahack[sizeof(Base)] { };
    const Base* base = reinterpret_cast<const Base*>(ahack);
    return reinterpret_cast<std::ptrdiff_t>(&(base->*ptr))
         - reinterpret_cast<std::ptrdiff_t>(base);
}

template <typename T, typename U>
GLOX_ALWAYS_INLINE inline const T*
container_of(const U* const a, U const T::* const ptr)
{
    return reinterpret_cast<const T*>(
        reinterpret_cast<const char*>(a) - offset_of(ptr)
    );
}
template <typename T, typename U>
GLOX_ALWAYS_INLINE inline T* container_of(U* a, U T::* ptr)
{
    return reinterpret_cast<T*>(reinterpret_cast<char*>(a) - offset_of(ptr));
}
GLOX_END_EXPORT
} // namespace glox
