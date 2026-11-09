#pragma once
#ifndef USE_MODULES
#include "assert.hpp"
#include "detail/moveutils.hpp"
#include "metaprog.hpp"
#include "option.hpp"
#include "string.hpp"
#include "types.hpp"
#include <string_view>
#endif

GLOX_EXPORT
namespace glox {

class path_view
{
    std::string_view str;

public:
    constexpr path_view() noexcept = default;
    constexpr path_view(std::string_view s) noexcept
        : str(s)
    {
    }
    constexpr path_view(const char* s) noexcept
        : str(s)
    {
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return str.empty();
    }
    [[nodiscard]] constexpr const char* data() const noexcept
    {
        return str.data();
    }
    [[nodiscard]] constexpr size_t size() const noexcept
    {
        return str.size();
    }
    [[nodiscard]] constexpr decltype(auto) begin() noexcept
    {
        return str.begin();
    }
    [[nodiscard]] constexpr decltype(auto) begin() const noexcept
    {
        return str.cbegin();
    }
    [[nodiscard]] constexpr decltype(auto) end() noexcept
    {
        return str.end();
    }
    [[nodiscard]] constexpr decltype(auto) end() const noexcept
    {
        return str.cend();
    }

    [[nodiscard]] constexpr path_view filename() const noexcept
    {
        auto pos = str.find_last_of('/');
        if (pos == std::string_view::npos)
            return *this;
        return path_view { str.substr(pos + 1) };
    }

    [[nodiscard]] constexpr path_view parent_path() const noexcept
    {
        auto pos = str.find_last_of('/');
        if (pos == std::string_view::npos)
            return path_view { };
        if (pos == 0)
            return path_view { str.substr(0, 1) }; // root "/"
        return path_view { str.substr(0, pos) };
    }

    [[nodiscard]] constexpr path_view extension() const noexcept
    {
        auto fn = filename().str;
        auto pos = fn.find_last_of('.');
        // Handle no extension, or hidden dotfiles like ".config"
        if (pos == std::string_view::npos || pos == 0)
            return path_view { };
        return path_view { fn.substr(pos) };
    }

    [[nodiscard]] constexpr path_view stem() const noexcept
    {
        auto fn = filename().str;
        auto pos = fn.find_last_of('.');
        if (pos == std::string_view::npos || pos == 0)
            return path_view { fn };
        return path_view { fn.substr(0, pos) };
    }

    [[nodiscard]] constexpr bool is_absolute() const noexcept
    {
        return !str.empty() && str[0] == '/';
    }

    friend constexpr bool operator==(path_view lhs, path_view rhs) noexcept
    {
        return lhs.str == rhs.str;
    }

    friend constexpr bool operator!=(path_view lhs, path_view rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

template <glox::allocator<char> Allocator = glox::default_allocator<char>>
class path
{
    string<Allocator> str;

public:
    static constexpr char PREFERRED_SEPARATOR = '/';

    constexpr path() = default;

    constexpr explicit path(path_view pv)
        : str(pv.data(), pv.size())
    {
    }
    constexpr path(const char* s)
        : str(s)
    {
    }
    constexpr path(string<Allocator> s)
        : str(std::move(s))
    {
    }

    constexpr operator path_view() const noexcept
    {
        return path_view { std::string_view(str.data(), str.size()) };
    }

    [[nodiscard]] constexpr path_view view() const noexcept
    {
        return *this;
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return str.empty();
    }
    [[nodiscard]] constexpr const char* c_str() const noexcept
    {
        return str.c_str();
    }

    constexpr path& operator/=(path_view rhs)
    {
        if (rhs.empty())
            return *this;

        if (rhs.is_absolute()) {
            str.clear();
        } else if (!empty()) {
            // Use .data() instead of .c_str()
            if (str.data()[str.size() - 1] != PREFERRED_SEPARATOR) {
                str.push_back(PREFERRED_SEPARATOR);
            }
        }

        for (char c : rhs) {
            str.push_back(c);
        }

        return *this;
    }
    constexpr friend path operator/(const path& lhs, path_view rhs)
    {
        path result = lhs;
        result /= rhs;
        return result;
    }

    // Delegate parsers to path_view for zero-allocation
    [[nodiscard]] constexpr path_view filename() const noexcept
    {
        return view().filename();
    }
    [[nodiscard]] constexpr path_view parent_path() const noexcept
    {
        return view().parent_path();
    }
    [[nodiscard]] constexpr path_view extension() const noexcept
    {
        return view().extension();
    }
    [[nodiscard]] constexpr path_view stem() const noexcept
    {
        return view().stem();
    }
    [[nodiscard]] constexpr bool is_absolute() const noexcept
    {
        return view().is_absolute();
    }
    [[nodiscard]] constexpr decltype(auto) begin() noexcept
    {
        return str.begin();
    }
    [[nodiscard]] constexpr decltype(auto) begin() const noexcept
    {
        return str.cbegin();
    }
    [[nodiscard]] constexpr decltype(auto) end() noexcept
    {
        return str.end();
    }
    [[nodiscard]] constexpr decltype(auto) end() const noexcept
    {
        return str.cend();
    }
};

// Equality operators
template <glox::allocator<char> Allocator>
constexpr bool operator==(const path<Allocator>& lhs, path_view rhs) noexcept
{
    return lhs.view() == rhs;
}

template <glox::allocator<char> Allocator>
constexpr bool operator==(path_view lhs, const path<Allocator>& rhs) noexcept
{
    return lhs == rhs.view();
}

template <glox::allocator<char> Allocator1, glox::allocator<char> Allocator2>
constexpr bool operator==(
    const path<Allocator1>& lhs,
    const path<Allocator2>& rhs
) noexcept
{
    return lhs.view() == rhs.view();
}

} // namespace glox
