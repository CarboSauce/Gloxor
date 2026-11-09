#pragma once
#include <limits.h>
#ifndef USE_MODULES
#include "detail/moveutils.hpp"
#include "glox/alloc.hpp"
#include "glox/string.h"
#include "glox/types.hpp"
#include <string_view>
#endif

GLOX_EXPORT
namespace glox {

template <glox::allocator<char> Allocator = glox::default_allocator<char>>
class string
{
    struct big_str
    {
        struct [[gnu::packed]]
        {
            size_t is_big : 1;
            size_t cap : sizeof(char) * CHAR_BIT - 1;
        };
        size_t size;
        char* data;

        constexpr big_str() = default;
    };

    static constexpr size_t MINIMAL_CAP = sizeof(big_str) - 1;

    struct short_str
    {
        struct [[gnu::packed]]
        {
            unsigned char is_big : 1;
            unsigned char size : 7;
        };
        char data[MINIMAL_CAP];
    };

    static_assert(sizeof(big_str) == sizeof(short_str));

    union repr
    {
        short_str s;
        big_str b;

        constexpr repr() = default;
        constexpr repr(big_str b)
            : b(b)
        {
        }
        constexpr repr(short_str s)
            : s(s)
        {
        }
    };

    repr r;
    [[no_unique_address]] Allocator alloc;

    constexpr bool is_big() const
    {
        if (std::is_constant_evaluated() and __builtin_constant_p(r.b.is_big)) {
            return r.b.is_big;
        } else {
            return r.s.is_big;
        }
    }

    [[gnu::always_inline]] constexpr bool fits_small(size_t s) const
    {
        return s <= MINIMAL_CAP;
    }

#ifdef LITTLE_ENDIAN
    static constexpr size_t CAP_FACTOR = 1;
#else
    static constexpr size_t CAP_FACTOR = 2;
#endif

    constexpr char* init(size_t s)
    {
        if (fits_small(s)) {
            r.s.size = s;
            return r.s.data;
        } else {
            ensure_can_fit(s);
            return r.b.data;
        }
    }

public:
    using allocator = Allocator;
    using size_type = std::size_t;
    using reference = char&;
    using const_reference = const char&;
    using iterator = char*;
    using const_iterator = const char*;

    constexpr string()
        : r()
        , alloc()
    {
    }

    constexpr string(size_t s)
        : r()
        , alloc()
    {
        init(s);
    }

    constexpr string(const char* str)
        : r()
    {
        size_t n = glox::strlen(str);
        ensure_can_fit(n);
        for (char c : std::string_view { str, n }) {
            push_back(c);
        }
    }

    constexpr string(const char* str, size_t n)
        : r()
    {
        reserve(n);
        for (size_t i = 0; i != n; i++)
            push_back(str[i]);
    }

    constexpr string(std::string_view str)
        : r()
    {
        size_t n = str.length();
        reserve(n);
        for (char c : str)
            push_back(c);
    }

    constexpr string(const string& other)
        : alloc(other.alloc)
    {
        if (not other.is_big()) {
            r = other.r;
        } else {
            size_t n = other.size();
            reserve(n);
            for (char c : other) {
                push_back(c);
            }
        }
    }

    constexpr string& operator=(const string& other)
    {
        if (this == &other) {
            return *this;
        }

        alloc = other.alloc;

        if (not is_big() && not other.is_big()) {
            r = other.r;
        } else {
            size_t n = other.size();
            reserve(n);
            clear();
            for (char c : other) {
                push_back(c);
            }
        }

        return *this;
    }
    constexpr string& operator=(string&& other)
    {
        using glox::swap;
        swap(alloc, other.alloc);
        swap(r, other.r);
        return *this;
    }

    constexpr string(string&& other)
        : r(RVALUE(other.r))
        , alloc(RVALUE(other.alloc))
    {
        other.r = { };
    }

    constexpr ~string()
    {
        if (is_big()) {
            glox::dealloc(alloc, r.b.data, r.b.cap);
        }
    }

    constexpr char* data()
    {
        return is_big() ? r.b.data : r.s.data;
    }

    constexpr const char* data() const
    {
        return is_big() ? r.b.data : r.s.data;
    }

    constexpr size_t capacity() const
    {
        return is_big() ? r.b.cap * CAP_FACTOR : MINIMAL_CAP;
    }

    constexpr iterator begin()
    {
        return data();
    }
    constexpr const_iterator begin() const
    {
        return data();
    }

    constexpr iterator end()
    {
        return data() + size();
    }
    constexpr const_iterator end() const
    {
        return data() + size();
    }

    constexpr const_iterator cbegin() const
    {
        return data();
    }
    constexpr const_iterator cbegin()
    {
        return data();
    }

    constexpr const_iterator cend() const
    {
        return data() + size();
    }
    constexpr const_iterator cend()
    {
        return data() + size();
    }

    constexpr auto empty() const
    {
        return size() == 0;
    }

    constexpr size_t size() const
    {
        return is_big() ? r.b.size : r.s.size;
    }
    constexpr size_t len() const
    {
        ;
        return size();
    }

    constexpr const_reference operator[](size_t i) const
    {
        GLOX_ASSERT(i < size());
        return *(data() + i);
    }
    constexpr reference operator[](size_t i)
    {
        GLOX_ASSERT(i < size());
        return *(data() + i);
    }

    constexpr const_reference back() const
    {
        GLOX_ASSERT(size() != 0);
        return *(data() + size() - 1);
    }
    constexpr reference back()
    {
        GLOX_ASSERT(size() != 0);
        return *(data() + size() - 1);
    }
    constexpr const_reference front() const
    {
        GLOX_ASSERT(size() != 0);
        return *data();
    }
    constexpr reference front()
    {
        GLOX_ASSERT(size() != 0);
        return *data();
    }

    constexpr std::string_view view() const
    {
        return std::string_view { data(), size() };
    }

    constexpr bool operator==(const string&) const = default;
    constexpr auto operator<=>(const string& other) const
    {
        return view() <=> other.view();
    }

    // friend constexpr auto operator<=>(std::string_view l, const string& r)
    // {
    //     return l <=> r.view();
    // }
    template <std::convertible_to<std::string_view> T>
    constexpr auto operator<=>(const T& l) const
    {
        return std::string_view(l) <=> this->view();
    }
    template <std::convertible_to<std::string_view> T>
    constexpr auto operator==(const T& l) const
    {
        return std::string_view(l) == this->view();
    }

    constexpr auto push_back(char c)
    {
        auto siz = size();
        if (not ensure_can_fit(siz + 1))
            return false;
        data()[siz] = c;
        set_size(siz + 1);

        return true;
    }

    constexpr auto emplace_back(char c)
    {
        return push_back(c);
    }

    constexpr void pop_back()
    {
        GLOX_ASSERT(size() > 0);
        set_size(size() - 1);
    }

    template <std::convertible_to<std::string_view> T>
    constexpr void append(const T& str)
    {
        auto view = std::string_view(str);
        reserve(capacity() + view.size());
        for (char c : view) {
            push_back(c);
        }
    }

    constexpr iterator insert(const_iterator pos, char val)
    {
        const auto d = data();
        const auto index = pos - d;
        if (pos == end()) {
            push_back(val);
            return d + index;
        }
        push_back('\0');
        auto mutIter = data() + index;
        glox::move_range_backward(mutIter, end() - 1, end());
        *mutIter = val;
        return mutIter;
    }

    constexpr iterator erase(const_iterator pos)
    {
        const auto d = data();
        const auto p = d + (pos - d);
        glox::move_range(p + 1, end(), p);
        pop_back();
        return p;
    }

    constexpr auto reserve(size_t s)
    {
        ensure_can_fit(s);
    }

    constexpr void clear()
    {
        set_size(0);
    }

private:
    constexpr void set_capacity(size_t cap)
    {
        if (is_big()) {
            r.b.cap = cap / CAP_FACTOR;
        }
    }

    constexpr void set_size(size_t s)
    {
        if (is_big()) {
            r.b.size = s;
        } else {
            r.s.size = s;
        }
    }

    constexpr alloc_handle<char> realloc_buffer(size_t old, size_t news)
    {
        if (is_big()) {
            return glox::grow_alloc(
                alloc, data(), old, news, old, alignof(char)
            );
        } else {
            return glox::alloc_uninitialized<char>(alloc, news);
        }
    }

    constexpr void set_data(char* d)
    {
        if (is_big()) {
            r.b.data = d;
        }
    }

    constexpr void begin_lifetime(char* p, size_t from, size_t to)
    {
        if consteval {
            for (; from != to; from++) {
                new (p + from) char('\0');
            }
        }
    }

    constexpr bool ensure_can_fit(size_t new_size)
    {
        const auto cap = capacity();
        if (new_size > cap) {
            new_size = cap < 32 ? 32 : cap + cap / 2;
            const auto mem = realloc_buffer(cap, new_size);
            if (not mem.ptr)
                return false;
            if (not is_big()) {
                begin_lifetime(mem.ptr, 0, mem.count);
                const size_t sz = r.s.size;
                for (size_t i = 0; i != sz; ++i)
                    mem.ptr[i] = r.s.data[i];
                r.b = big_str();
                r.b.is_big = 1;
                r.b.size = sz;
            }
            r.b.data = mem.ptr;
            r.b.cap = mem.count / CAP_FACTOR;
        }

        return true;
    }
};
template <glox::allocator<char> Allocator = glox::default_allocator<char>>
class cstring
{
    glox::string<Allocator> inner;

    constexpr void init_null()
    {
        if (inner.empty() || inner.back() != '\0') {
            inner.push_back('\0');
        }
    }

public:
    using size_type = std::size_t;
    using reference = char&;
    using const_reference = const char&;
    using iterator = char*;
    using const_iterator = const char*;

    constexpr cstring()
        : inner()
    {
        inner.push_back('\0');
    }

    constexpr cstring(const char* s)
        : inner(s)
    {
        inner.push_back('\0');
    }

    constexpr cstring(const char* s, size_t n)
        : inner(s, n)
    {
        inner.push_back('\0');
    }

    constexpr cstring(std::string_view sv)
        : inner(sv)
    {
        inner.push_back('\0');
    }

    constexpr explicit cstring(glox::string<Allocator> s)
        : inner(std::move(s))
    {
        init_null();
    }

    [[nodiscard]] constexpr size_t size() const noexcept
    {
        return inner.size() - 1; // Mask the \0
    }

    [[nodiscard]] constexpr size_t len() const noexcept
    {
        return size();
    }
    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]] constexpr const char* c_str() const noexcept
    {
        return inner.data();
    }

    [[nodiscard]] constexpr const char* data() const noexcept
    {
        return inner.data();
    }
    [[nodiscard]] constexpr char* data() noexcept
    {
        return inner.data();
    }

    constexpr void push_back(char c)
    {
        // Replace current \0 with new char, then append a new \0
        inner.back() = c;
        inner.push_back('\0');
    }

    constexpr void pop_back()
    {
        GLOX_ASSERT(size() > 0);
        inner.pop_back(); // Remove \0
        inner.back() = '\0'; // Replace last actual char with \0
    }

    constexpr void clear()
    {
        inner.clear();
        inner.push_back('\0');
    }

    template <std::convertible_to<std::string_view> T>
    constexpr void append(const T& str)
    {
        std::string_view view(str);
        if (view.empty())
            return;

        inner.pop_back(); // Remove trailing \0
        inner.append(view); // Append new data
        inner.push_back('\0'); // Restore trailing \0
    }

    template <std::convertible_to<std::string_view> T>
    constexpr cstring& operator+=(const T& str)
    {
        append(str);
        return *this;
    }

    constexpr void reserve(size_t s)
    {
        inner.reserve(s + 1); // Account for \0
    }

    constexpr const_reference operator[](size_t i) const
    {
        GLOX_ASSERT(i < size());
        return inner[i];
    }
    constexpr reference operator[](size_t i)
    {
        GLOX_ASSERT(i < size());
        return inner[i];
    }

    constexpr const_reference front() const
    {
        GLOX_ASSERT(size() > 0);
        return inner.front();
    }
    constexpr reference front()
    {
        GLOX_ASSERT(size() > 0);
        return inner.front();
    }

    constexpr const_reference back() const
    {
        GLOX_ASSERT(size() > 0);
        return inner[size() - 1]; // Avoid returning the \0
    }
    constexpr reference back()
    {
        GLOX_ASSERT(size() > 0);
        return inner[size() - 1]; // Avoid returning the \0
    }

    constexpr iterator begin()
    {
        return inner.begin();
    }
    constexpr const_iterator begin() const
    {
        return inner.begin();
    }
    constexpr const_iterator cbegin() const
    {
        return inner.cbegin();
    }

    constexpr iterator end()
    {
        return inner.begin() + size();
    }
    constexpr const_iterator end() const
    {
        return inner.begin() + size();
    }
    constexpr const_iterator cend() const
    {
        return inner.cbegin() + size();
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept
    {
        return std::string_view(data(), size());
    }

    template <std::convertible_to<std::string_view> T>
    constexpr auto operator<=>(const T& rhs) const
    {
        return view() <=> std::string_view(rhs);
    }

    template <std::convertible_to<std::string_view> T>
    constexpr bool operator==(const T& rhs) const
    {
        return view() == std::string_view(rhs);
    }

    [[nodiscard]] constexpr glox::string<Allocator> to_string() const
    {
        return glox::string<Allocator>(view());
    }
};

} // namespace glox
