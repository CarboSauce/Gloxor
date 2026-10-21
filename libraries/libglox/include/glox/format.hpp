#pragma once
#ifndef USE_MODULES
#include "glox/array.hpp"
#include "glox/math.hpp"
#include "glox/string.hpp"
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

enum class format_options
{
    def,
    hex,
    bin,
    dec,
};

template <typename T>
struct formatter;

template <std::unsigned_integral T>
struct formatter<T>
{
    void format(formattable_to<char> auto& buffer, format_options opts, T value)
    {
        char buf[64];
        size_t i = 0;
        if (opts == format_options::dec or opts == format_options::def) {
            do {
                auto digit = value % 10;
                value /= 10;
                buf[i++] = '0' + digit;
            } while (value > 0);
        } else if (opts == format_options::hex) {
            buffer.write('0');
            buffer.write('x');
            do {
                auto digit = value % 16;
                value /= 16;
                char hexDigit = "0123456789ABCDEF"[digit];
                buf[i++] = hexDigit;
            } while (value > 0);
        } else if (opts == format_options::bin) {
            buffer.write('0');
            buffer.write('b');
            do {
                auto digit = value & 1;
                value >>= 1;
                buf[i++] = digit ? '1' : '0';
            } while (value > 0);
        }
        for (; --i != (size_t)-1;) {
            buffer.write(buf[i]);
        }
    }
};

template <std::signed_integral T>
struct formatter<T>
{
    void format(formattable_to<char> auto& buffer, format_options opts, T value)
    {
        uintmax_t uvalue = value;
        if (value < 0) {
            buffer.write('-');
            uvalue = -(uintmax_t)value;
        }
        return glox::formatter<decltype(uvalue)> { }.format(
            buffer, opts, uvalue
        );
    }
};

template <>
struct formatter<std::string_view>
{
    void format(
        formattable_to<char> auto& buffer,
        [[maybe_unused]] format_options opts,
        std::string_view value
    )
    {
        for (const auto c : value) {
            buffer.write(c);
        }
    }
};

template <>
struct formatter<const char*>
{
    void format(
        formattable_to<char> auto& buffer,
        [[maybe_unused]] format_options opts,
        const char* value
    )
    {
        while (char c = *value++) {
            buffer.write(c);
        }
    }
};

template <glox::allocator<char> Alloc>
struct formatter<glox::string<Alloc>>
{
    void format(
        formattable_to<char> auto& buffer,
        [[maybe_unused]] format_options opts,
        const glox::string<Alloc>& value
    )
    {
        for (const auto c : value) {
            buffer.write(c);
        }
    }
};

template <typename T>
    requires std::is_pointer_v<T>
struct formatter<T>
{
    void
    format(formattable_to<char> auto& buffer, format_options opts, void* value)
    {
        opts = opts == format_options::def ? format_options::hex : opts;
        return glox::formatter<uintptr_t> { }.format(
            buffer, opts, reinterpret_cast<uintptr_t>(value)
        );
    }
};

template <formattable_to<char> Buffer, typename... Args>
constexpr auto format_to(Buffer& buffer, std::string_view templ, Args&&... args)
{
    auto beg = templ.begin();
    const auto end = templ.end();

    auto next = [&]() -> char { return beg == end ? '\0' : *beg++; };
    auto peek = [&]() -> char { return beg == end ? '\0' : *beg; };

    auto parse = [&](auto&& arg) {
        while (beg != end) {
            char c = next();

            if (c != '%') {
                buffer.write(c);
                continue;
            }

            if (peek() == '%') {
                next();
                buffer.write('%');
                continue;
            }

            format_options opts = format_options::def;

            char cpeeked = peek();

            if (cpeeked == 's') {
                next();
                opts = format_options::def;
            } else if (cpeeked == 'd') {
                next();
                opts = format_options::dec;
            } else if (cpeeked == 'b') {
                next();
                opts = format_options::bin;
            } else if (cpeeked == 'x') {
                next();
                opts = format_options::hex;
            }

            glox::formatter<std::decay_t<decltype(arg)>> { }.format(
                buffer, opts, FORWARD(arg)
            );

            return;
        }
    };

    (parse(FORWARD(args)), ...);

    while (beg != templ.end()) {
        buffer.write(*beg++);
    }

    return buffer;
}

GLOX_END_EXPORT
} // namespace glox
