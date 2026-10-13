#pragma once
#ifndef USE_MODULES
#include "alloc.hpp"
#include "detail/memory.hpp"
#include "detail/moveutils.hpp"
#include "option.hpp"
#endif

#include "assert.hpp"

namespace glox {
GLOX_BEGIN_EXPORT
template <typename T, glox::allocator<T> Allocator = glox::default_allocator<T>>
class [[clang::trivial_abi]] box
{
    T* ptr;
    [[no_unique_address]] Allocator alloc;

public:
    using allocator = Allocator;
    using reference = T&;
    using pointer = T*;

    constexpr box()
        : ptr(nullptr)
        , alloc(Allocator { })
    {
    }
    constexpr box(auto&&... args)
        : ptr(glox::alloc_uninitialized<T>(alloc, 1).ptr)
        , alloc(Allocator { })
    {
        if (ptr != nullptr) {
            std::construct_at(ptr, FORWARD(args)...);
        }
    }

    box(const box&) = delete;
    box& operator=(const box&) = delete;

    constexpr box(box&& other)
        : ptr(glox::exchange(other.ptr, nullptr))
        , alloc(RVALUE(other.alloc))
    {
    }
    constexpr box& operator=(box&& other)
    {
        using glox::swap;
        swap(ptr, other.ptr);
        swap(this->alloc, other->alloc);
    }

    constexpr bool is_null() const
    {
        return ptr == nullptr;
    }

    constexpr ~box()
    {
        if (ptr != nullptr) {
            ptr->~T();
            glox::dealloc(alloc, ptr, 1);
            ptr = nullptr;
        }
    }

    constexpr static glox::option<box> create(auto&&... args)
    {
        glox::option<box> val(FORWARD(args)...);
        if (val.val().is_null()) {
            return { };
        } else
            return val;
    }
    constexpr auto get_allocator() const
    {
        return alloc;
    }

    constexpr glox::option<T&> get() const
    {
        if (ptr != nullptr) {
            return ptr;
        } else {
            return { };
        }
    }

    constexpr T* get_ptr() const
    {
        return ptr;
    }

    constexpr std::add_lvalue_reference_t<T> operator*() const
    {
        GLOX_ASSERT(ptr != nullptr);
        return *ptr;
    }

    constexpr pointer operator->() const
    {
        return ptr;
    }

    constexpr operator bool() const
    {
        return ptr != nullptr;
    }
};

template <typename T>
box(T) -> box<std::remove_reference_t<T>>;
GLOX_END_EXPORT
} // namespace glox
