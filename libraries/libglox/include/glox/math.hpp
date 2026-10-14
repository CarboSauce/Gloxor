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

constexpr uint64_t log2(uint64_t x)
{
    return x ? 64u - __builtin_clzll(x) : 0;
}

/*
 * @brief Calculates log10 of integer
 */
constexpr uint64_t log10(uint64_t x)
{
    // clang-format off
    static constexpr unsigned char table[] = {
        0, 0, 0, 0, 1, 1, 1, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 5, 5, 5,
        6, 6, 6, 6, 7, 7, 7, 8, 8, 8,
        9, 9, 9, 9, 10, 10, 10, 11, 11, 11,
        12, 12, 12, 12, 13, 13, 13, 14, 14, 14,
        15, 15, 15, 15, 16, 16, 16, 17, 17, 17,
        18, 18, 18, 18, 19,
    };

    static constexpr std::uint64_t powersOfTen[] = {
        1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL,
        100000ULL, 1000000ULL, 10000000ULL,
        100000000ULL, 1000000000ULL,
        10000000000ULL, 100000000000ULL,
        1000000000000ULL, 10000000000000ULL,
        100000000000000ULL, 1000000000000000ULL,
        10000000000000000ULL, 100000000000000000ULL,
        1000000000000000000ULL, 10000000000000000000ULL
    };
    // clang-format on

    auto digits = table[glox::log2(x)];
    return digits + (x >= powersOfTen[digits]);
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
