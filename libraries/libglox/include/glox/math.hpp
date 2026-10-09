#pragma once
#include "types.hpp"

#define ALLIGNUP(val, to) ALIGN(val + to, to)
#define ALIGN(val, to) ((val) & (~(to - 1)))
namespace glox {
GLOX_BEGIN_EXPORT
constexpr uint32_t align_up(uint32_t x, uint32_t to)
{
    return ((x + to) & (~(to - 1)));
}

constexpr uint32_t align(uint32_t x, uint32_t to)
{
    return ((x) & (~(to - 1)));
}
/**
 *	@brief Count leading zeros
 */
constexpr uint32_t clz(uint32_t x)
{
    return __builtin_clz(x);
}
/**
 *	@brief Count leading zeros
 */
constexpr uint64_t clz(uint64_t x)
{
    return __builtin_clzll(x);
}
/**
 * @brief Rounds input to next power of 2
 * @param x Number to round up, must be above 0
 */
constexpr uint32_t next_pow2(uint32_t x)
{
    if (x == 1)
        return 1;
    return 1 << (32 - __builtin_clz(x - 1));
}
/**
 * @brief Rounds input to next power of 2
 * @param x Number to round up, must be above 0
 */
constexpr uint64_t next_pow2(uint64_t x)
{
    if (x == 1)
        return 1;
    return 1 << (64 - __builtin_clzll(x - 1));
}
GLOX_END_EXPORT
} // namespace glox
