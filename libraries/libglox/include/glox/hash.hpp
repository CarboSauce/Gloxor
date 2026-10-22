#pragma once
#include <type_traits>
#ifndef USE_MODULES
#include "glox/types.hpp"
#include <concepts>
#include <string_view>
#endif

GLOX_EXPORT
namespace glox {
constexpr auto djb2_hash(const char* str)
{
    uint32_t hash = 5381; // magic prime number
    uint8_t c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c;
    return hash;
}
constexpr auto djb2_hash(std::string_view str)
{
    uint32_t hash = 5381; // magic prime number
    for (char c : str)
        hash = ((hash << 5) + hash) ^ c;
    return hash;
}

template <typename T>
struct hash;

template <typename T>
    requires std::signed_integral<T> or std::unsigned_integral<T>
struct hash<T>
{
    constexpr auto operator()(T val) const
    {
        return val;
    }
};

template <>
struct hash<const char*>
{
    constexpr auto operator()(const char* val) const
    {
        return djb2_hash(val);
    }
};

template <>
struct hash<std::string_view>
{
    constexpr auto operator()(std::string_view str) const
    {
        return djb2_hash(str);
    }
};

template <typename T>
    requires std::is_pointer_v<T>
struct hash<T>
{
    constexpr auto operator()(const T* val) const
    {
        return reinterpret_cast<uintptr_t>(val);
    }
};

} // namespace glox
