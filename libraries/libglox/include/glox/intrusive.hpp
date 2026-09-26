#pragma once

#include "glox/macros.hpp"
#include <cstddef>

template <typename Base, typename Member>
GLOX_ALWAYS_INLINE inline std::ptrdiff_t offset_of(const Member Base::* ptr)
{
    alignas(Base) char ahack[sizeof(Base)] { };
    const Base* base = reinterpret_cast<const Base*>(ahack);
    return reinterpret_cast<std::ptrdiff_t>(&(base->*ptr))
         - reinterpret_cast<std::ptrdiff_t>(base);
}
