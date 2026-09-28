#pragma once
#include "alloc.hpp"
#include "assert.hpp"
#include "detail/moveutils.hpp"
#include "result.hpp"

namespace glox {
// TODO: Factory based constructors
// More member funcs
template <typename T, glox::allocator Allocator = glox::default_allocator>
class vector
{
    T* start = nullptr;
    size_t cap = 0, siz = 0;
    [[no_unique_address]] Allocator alloc;

public:
    using allocator = Allocator;
    constexpr vector() = default;
    constexpr vector(size_t reserve)
    {
        start = glox::alloc_uninitialized<T>(alloc, reserve).ptr;
        if (!start)
            cap = 0;
        else
            cap = reserve;
        siz = 0;
    }
    constexpr vector(const T& val, size_t size)
    {
        start = glox::alloc_uninitialized<T>(alloc, size).ptr;
        if (!start)
            return;
        cap = size;
        siz = size;
        for (size_t i = 0; i < size; ++i) {
            ::new (start + i) T(val);
        }
    }
    constexpr vector(const vector& other)
        : alloc(other.alloc)
    {
        start = glox::alloc_uninitialized<T>(alloc, other.cap).ptr;
        if (!start)
            return;
        cap = other.cap;
        siz = other.siz;
        for (size_t i = 0; i < siz; ++i) {
            ::new (start + i) T(other.start[i]);
        }
    }
    constexpr vector& operator=(const vector& other)
    {
        start = realloc_buffer(cap, other.cap);
        if (!start) {
            siz = cap = 0;
            return *this;
        }
        cap = other.cap;
        siz = other.siz;
        for (size_t i = 0; i < siz; ++i) {
            ::new (start + i) T(other.start[i]);
        }
    }
    constexpr vector(vector&& other)
        : alloc(RVALUE(other.alloc))
    {
        start = other.start;
        other.start = nullptr;
        cap = other.cap;
        other.cap = 0;
        siz = other.siz;
        other.siz = 0;
    }
    constexpr vector& operator=(vector&& other)
    {
        using glox::swap;
        swap(start, other.start);
        swap(static_cast<Allocator&>(*this), static_cast<Allocator&>(other));
        cap = other.cap;
        other.cap = 0;
        siz = other.siz;
        other.siz = 0;
    }
    constexpr ~vector()
    {
        for (size_t i = 0; i != siz; ++i) {
            start[i].~T();
        }
        glox::dealloc(alloc, start, cap);
    }
    constexpr static glox::result<glox::vector<T>, option_t>
    with_capacity(size_t cap)
    {
        glox::vector<T> tmp(cap);
        if (tmp.is_null())
            return option_t::none;
        else
            return tmp;
    }
    constexpr auto begin()
    {
        return start;
    }
    constexpr auto end()
    {
        return start + siz;
    }
    constexpr auto& back()
    {
        return start[siz - 1];
    }
    constexpr const auto& back() const
    {
        return start[siz - 1];
    }
    constexpr auto& front()
    {
        return start[0];
    }
    constexpr const auto& front() const
    {
        return start[0];
    }
    constexpr auto begin() const
    {
        return start;
    }
    constexpr auto end() const
    {
        return start + siz;
    }
    constexpr auto size() const
    {
        return siz;
    }
    constexpr auto capacity() const
    {
        return cap;
    }
    constexpr auto empty() const
    {
        return siz == 0;
    }
    constexpr auto is_null() const
    {
        return start == nullptr;
    }

    template <typename... Args>
    constexpr bool emplace_back(Args&&... args)
    {
        if (!ensure_can_fit(siz + 1))
            return false;
        std::construct_at(start + siz++, FORWARD(args)...);
        return true;
    }
    constexpr bool reserve(size_t new_cap)
    {
        if (cap < new_cap)
            return realloc_buffer(cap, new_cap);
        return true;
    }
    constexpr void pop_back()
    {
        gloxAssert(siz > 0);
        start[--siz].~T();
    }
    constexpr const T& operator[](size_t i) const
    {
        gloxAssert(i < size);
        return *(start + i);
    }
    constexpr T& operator[](size_t i)
    {
        return const_cast<T&>(static_cast<const vector&>(*this)[i]);
    }

private:
    constexpr auto* realloc_buffer(size_t old, size_t news)
    {

        if (start != nullptr) {
            return glox::grow_alloc(
                alloc, start, old, alignof(T), news, alignof(T)
            )
                .ptr;
        } else {
            return glox::alloc_uninitialized<T>(alloc, news).ptr;
        }
    }
    /*
     * @brief Expands vector to fit new_size index
     * @param new_size new allocation size
     */
    constexpr bool ensure_can_fit(size_t new_size)
    {
        if (new_size > cap) {
            new_size = cap < 4 ? 4 : cap + cap / 2; // siz = siz * 1.5;
            auto tmpPtr = realloc_buffer(cap, new_size);
            if (!tmpPtr)
                return false;
            start = tmpPtr;
            cap = new_size;
        }
        return true;
    }
};
} // namespace glox
