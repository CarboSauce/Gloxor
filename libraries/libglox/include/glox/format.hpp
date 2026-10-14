#pragma once
#ifndef USE_MODULES
#include "glox/array.hpp"
#include "glox/math.hpp"
#include "glox/types.hpp"
#include "glox/utilalgs.hpp"
#include <concepts>
#include <string_view>
#endif

namespace glox {
GLOX_BEGIN_EXPORT
namespace detail {
    inline auto format(char* buffer, const void* addr)
    {
        auto value = (uintptr_t)addr;
        size_t i = 0;
        buffer[i++] = '0';
        buffer[i++] = 'x';
        do {
            auto digit = value % 16;
            value /= 16;
            char hexDigit = "0123456789ABCDEF"[digit];
            buffer[i++] = hexDigit;
        } while (value > 0);
        glox::reverse(buffer + 2, buffer + i);
        return i;
    }

    inline auto format(char* buffer, uintmax_t value)
    {
        size_t i = 0;
        do {
            auto digit = value % 10;
            value /= 10;
            buffer[i++] = '0' + digit;
        } while (value > 0);
        glox::reverse(buffer, buffer + i);
        return i;
    }

    inline auto format(char* buffer, intmax_t value)
    {
        int signOffset = 0;
        uintmax_t uvalue = value;
        if (value < 0) {
            buffer[signOffset++] = '-';
            uvalue = -(uintmax_t)value;
        }
        return format(buffer + signOffset, uvalue) + signOffset;
    }

    inline auto format(char* buffer, char value)
    {
        *buffer = value;
        return (size_t)1;
    }

} // namespace detail

template <typename T, typename C>
concept formattable_to = requires(T& buffer, C val) {
    { buffer.write(val) } -> std::same_as<size_t>;
    { buffer.flush() } -> std::same_as<size_t>;
};

template <typename T>
struct formatter;

template <std::unsigned_integral T>
struct formatter<T>
{
    void format(formattable_to<char> auto& buffer, T value)
    {
        auto digCount = glox::log10(value);
        while (digCount > 0) {
            auto pten = (digCount * 10);
            auto c = value / pten;
            buffer.write('0' + c);
            value = value - c * pten;
            digCount--;
        }
    }
};

template <std::signed_integral T>
struct formatter<T>
{
    void format(formattable_to<char> auto& buffer, T value)
    {
        uintmax_t uvalue = value;
        if (value < 0) {
            buffer.write('-');
            uvalue = -(uintmax_t)value;
        }
        return glox::formatter<decltype(uvalue)> { }.format(buffer, uvalue);
    }
};

template <>
struct formatter<std::string_view>
{
    void format(formattable_to<char> auto& buffer, std::string_view value)
    {
        for (const auto c : value) {
            buffer.write(c);
        }
    }
};

template <>
struct formatter<const char*>
{
    void format(formattable_to<char> auto& buffer, const char* value)
    {
        while (char c = *value++) {
            buffer.write(c);
        }
    }
};

template <formattable_to<char> Buffer, typename... Args>
constexpr auto format_to(Buffer& buffer, std::string_view templ, Args&&... args)
{
    auto beg = templ.begin();
    auto parse = [&](auto&& arg) {
        while (beg != templ.end() && *beg != '{') {
            buffer.write(*beg++);
        }
        if (beg == templ.end())
            return;
        ++beg;

        if (beg == templ.end() || *beg != '}')
            return;

        glox::formatter<std::decay_t<decltype(arg)>> { }.format(
            buffer, FORWARD(arg)
        );
        ++beg;
    };

    (parse(FORWARD(args)), ...);

    while (beg != templ.end()) {
        buffer.write(*beg++);
    }

    return buffer;
}

GLOX_END_EXPORT
} // namespace glox
