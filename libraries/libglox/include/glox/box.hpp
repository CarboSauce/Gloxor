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

template <typename T, glox::allocator<T> Allocator>
class [[clang::trivial_abi]] box<T[], Allocator>
{
    T* ptr;
    std::size_t size_;
    [[no_unique_address]] Allocator alloc;

    constexpr void destroy_elements() noexcept
    {
        if (ptr != nullptr) {
            for (std::size_t i = 0; i < size_; ++i) {
                ptr[i].~T();
            }
            glox::dealloc(alloc, ptr, size_);
            ptr = nullptr;
            size_ = 0;
        }
    }

public:
    using allocator_type = Allocator;
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = std::size_t;

    constexpr box() noexcept
        : ptr(nullptr)
        , size_(0)
        , alloc(Allocator { })
    {
    }

    constexpr explicit box(std::size_t count)
        : ptr(nullptr)
        , size_(count)
        , alloc(Allocator { })
    {
        if (count == 0) {
            return;
        }
        ptr = glox::alloc_uninitialized<T>(alloc, count).ptr;
        if (ptr != nullptr) {
            std::size_t constructed = 0;
            try {
                for (; constructed < count; ++constructed) {
                    std::construct_at(ptr + constructed);
                }
            } catch (...) {
                for (std::size_t i = 0; i < constructed; ++i) {
                    ptr[i].~T();
                }
                glox::dealloc(alloc, ptr, count);
                ptr = nullptr;
                size_ = 0;
                throw;
            }
        } else {
            size_ = 0;
        }
    }

    constexpr box(std::size_t count, const T& value)
        : ptr(nullptr)
        , size_(count)
        , alloc(Allocator { })
    {
        if (count == 0) {
            return;
        }
        ptr = glox::alloc_uninitialized<T>(alloc, count).ptr;
        if (ptr != nullptr) {
            std::size_t constructed = 0;
            try {
                for (; constructed < count; ++constructed) {
                    std::construct_at(ptr + constructed, value);
                }
            } catch (...) {
                for (std::size_t i = 0; i < constructed; ++i) {
                    ptr[i].~T();
                }
                glox::dealloc(alloc, ptr, count);
                ptr = nullptr;
                size_ = 0;
                throw;
            }
        } else {
            size_ = 0;
        }
    }

    box(const box&) = delete;
    box& operator=(const box&) = delete;

    constexpr box(box&& other) noexcept
        : ptr(glox::exchange(other.ptr, nullptr))
        , size_(glox::exchange(other.size_, 0))
        , alloc(RVALUE(other.alloc))
    {
    }

    constexpr box& operator=(box&& other) noexcept
    {
        if (this != &other) {
            destroy_elements();
            ptr = glox::exchange(other.ptr, nullptr);
            size_ = glox::exchange(other.size_, 0);
            alloc = RVALUE(other.alloc);
        }
        return *this;
    }

    constexpr ~box()
    {
        destroy_elements();
    }

    constexpr static glox::option<box> create(std::size_t count)
    {
        box b(count);
        if (count > 0 && b.is_null()) {
            return { };
        }
        return b;
    }

    constexpr static glox::option<box> create(std::size_t count, const T& value)
    {
        box b(count, value);
        if (count > 0 && b.is_null()) {
            return { };
        }
        return b;
    }

    [[nodiscard]] constexpr bool is_null() const noexcept
    {
        return ptr == nullptr;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return ptr != nullptr;
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] constexpr auto get_allocator() const noexcept
    {
        return alloc;
    }

    [[nodiscard]] constexpr pointer get_ptr() noexcept
    {
        return ptr;
    }

    [[nodiscard]] constexpr const_pointer get_ptr() const noexcept
    {
        return ptr;
    }

    [[nodiscard]] constexpr reference operator[](std::size_t idx) noexcept
    {
        GLOX_ASSERT(ptr != nullptr && idx < size_);
        return ptr[idx];
    }

    [[nodiscard]] constexpr const_reference operator[](
        std::size_t idx
    ) const noexcept
    {
        GLOX_ASSERT(ptr != nullptr && idx < size_);
        return ptr[idx];
    }

    [[nodiscard]] constexpr reference front() noexcept
    {
        GLOX_ASSERT(ptr != nullptr && size_ > 0);
        return ptr[0];
    }

    [[nodiscard]] constexpr const_reference front() const noexcept
    {
        GLOX_ASSERT(ptr != nullptr && size_ > 0);
        return ptr[0];
    }

    [[nodiscard]] constexpr reference back() noexcept
    {
        GLOX_ASSERT(ptr != nullptr && size_ > 0);
        return ptr[size_ - 1];
    }

    [[nodiscard]] constexpr const_reference back() const noexcept
    {
        GLOX_ASSERT(ptr != nullptr && size_ > 0);
        return ptr[size_ - 1];
    }

    // Iterators
    [[nodiscard]] constexpr pointer begin() noexcept
    {
        return ptr;
    }
    [[nodiscard]] constexpr const_pointer begin() const noexcept
    {
        return ptr;
    }
    [[nodiscard]] constexpr const_pointer cbegin() const noexcept
    {
        return ptr;
    }

    [[nodiscard]] constexpr pointer end() noexcept
    {
        return ptr + size_;
    }
    [[nodiscard]] constexpr const_pointer end() const noexcept
    {
        return ptr + size_;
    }
    [[nodiscard]] constexpr const_pointer cend() const noexcept
    {
        return ptr + size_;
    }
};

template <typename T>
box(T) -> box<std::remove_reference_t<T>>;
GLOX_END_EXPORT
} // namespace glox
