#pragma once
#include "alloc.hpp"
#include "assert.hpp"
#include "detail/moveutils.hpp"
#include "result.hpp"

namespace glox {
// TODO: Factory based constructors
// More member funcs
template <typename T, glox::allocator<T> Allocator = glox::default_allocator<T>>
class vector
{
    T* start = nullptr;
    size_t cap = 0, siz = 0;
    [[no_unique_address]] Allocator alloc;

public:
    using allocator = Allocator;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;
    constexpr vector() = default;
    constexpr vector(size_t reserve)
    {
        const auto mem = glox::alloc_uninitialized<T>(alloc, reserve);
        if (!mem.ptr) {
            start = nullptr;
            cap = 0;
        } else {
            start = mem.ptr;
            cap = mem.count;
        }
        siz = 0;
    }
    constexpr vector(const T& val, size_t size)
    {
        const auto mem = glox::alloc_uninitialized<T>(alloc, size);
        if (!mem.ptr)
            return;
        start = mem.ptr;
        cap = mem.count;
        siz = size;
        for (size_t i = 0; i < size; ++i) {
            ::new (start + i) T(val);
        }
    }
    constexpr vector(const vector& other)
        : alloc(other.alloc)
    {
        const auto mem = glox::alloc_uninitialized<T>(alloc, other.cap);
        if (!mem.ptr)
            return;
        start = mem.ptr;
        cap = other.cap;
        siz = other.siz;
        for (size_t i = 0; i < siz; ++i) {
            ::new (start + i) T(other.start[i]);
        }
    }
    constexpr vector& operator=(const vector& other)
    {
        const auto mem = realloc_buffer(cap, other.cap);
        if (!mem.ptr) {
            start = nullptr;
            siz = cap = 0;
            return *this;
        }
        start = mem.ptr;
        cap = mem.count;
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
        clear();
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
    constexpr auto get_allocator() const
    {
        return alloc;
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
    constexpr iterator begin()
    {
        return start;
    }
    constexpr iterator end()
    {
        return start + siz;
    }
    constexpr const_iterator begin() const
    {
        return start;
    }
    constexpr const_iterator end() const
    {
        return start + siz;
    }
    constexpr const_iterator cbegin() const
    {
        return start;
    }
    constexpr const_iterator cend() const
    {
        return start + siz;
    }
    constexpr const_iterator cbegin()
    {
        return start;
    }
    constexpr const_iterator cend()
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
    constexpr auto shrink_to_fit()
    {
        const auto mem = glox::shrink_alloc(alloc, start, cap, siz, siz);
        if (mem.ptr == nullptr)
            return;
        start = mem.ptr;
        cap = mem.count;
    }

    template <typename... Args>
    constexpr bool push_back(Args&&... args)
    {
        return emplace_back(FORWARD(args)...);
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
        if (cap < new_cap) {
            const auto mem = realloc_buffer(cap, new_cap);
            if (mem.ptr == nullptr) {
                return false;
            }
            start = mem.ptr;
            cap = mem.count;
            return true;
        }
        return true;
    }
    constexpr void clear()
    {
        for (size_t i = 0; i != siz; ++i) {
            start[i].~T();
        }
        glox::dealloc(alloc, start, cap);
        start = nullptr;
        siz = 0;
        cap = 0;
    }
    constexpr iterator insert(const_iterator pos, T val)
    {
        const auto index = pos - start;
        if (pos == end()) {
            emplace_back(RVALUE(val));
            return start + index;
        }
        emplace_back();
        auto mutIter = start + index;
        glox::move_range_backward(mutIter, end() - 1, end());
        *mutIter = RVALUE(val);
        return mutIter;
    }
    constexpr iterator erase(const_iterator pos)
    {
        const auto p = start + (pos - start);
        glox::move_range(p + 1, end(), p);
        pop_back();
        return p;
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
        gloxAssert(i < size);
        return *(start + i);
    }

private:
    constexpr alloc_handle<T> realloc_buffer(size_t old, size_t news)
    {
        if (start != nullptr) {
            return glox::grow_alloc(alloc, start, old, news, old, alignof(T));
        } else {
            return glox::alloc_uninitialized<T>(alloc, news);
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
            const auto mem = realloc_buffer(cap, new_size);
            if (!mem.ptr)
                return false;
            start = mem.ptr;
            cap = mem.count;
        }
        return true;
    }
};
} // namespace glox
