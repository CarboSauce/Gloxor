#pragma once
#ifndef USE_MODULES
#include "alloc.hpp"
#include "detail/memory.hpp"
#include "detail/moveutils.hpp"
#include "option.hpp"
#include <type_traits>
#endif

#include "assert.hpp"

namespace glox {
GLOX_BEGIN_EXPORT
template <
    typename T,
    glox::allocator<std::remove_extent_t<T>> Allocator
    = glox::default_allocator<std::remove_extent_t<T>>
>
class box;

template <typename T, glox::allocator<std::remove_extent_t<T>> Allocator>
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

    template <typename... Args>
        requires(sizeof...(Args) > 0)
                 && (sizeof...(Args) != 1
                     || (!std::same_as<std::remove_cvref_t<Args>, box> && ...))
                 && std::constructible_from<T, Args...>
    constexpr box(Args&&... args)
        : ptr(glox::alloc_uninitialized<T>(alloc, 1).ptr)
        , alloc(Allocator { })
    {
        if (ptr != nullptr) {
            new (ptr) T(FORWARD(args)...);
        }
    }

    constexpr box(const box& other)
        requires std::is_copy_constructible_v<T>
                 and (not std::is_polymorphic_v<T>)
        : ptr(nullptr)
        , alloc(other.alloc)
    {
        if (other.ptr != nullptr) {
            ptr = glox::alloc_uninitialized<T>(this->alloc, 1).ptr;
            if (ptr != nullptr) {
                new (ptr) T(*other.ptr);
            }
        }
    }
    constexpr box& operator=(const box& other)
        requires std::is_copy_assignable_v<T> and (not std::is_polymorphic_v<T>)
    {
        using glox::swap;
        box tmp(other);
        swap(*this, tmp);

        return *this;
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
        swap(this->alloc, other.alloc);
        return *this;
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

    constexpr glox::option<const T&> get() const
    {
        if (ptr != nullptr) {
            return *ptr;
        } else {
            return { };
        }
    }

    constexpr glox::option<T&> get()
    {
        if (ptr != nullptr) {
            return *ptr;
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

template <typename T, glox::allocator<std::remove_extent_t<T>> Allocator>
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
            for (std::size_t i = 0; i < count; ++i) {
                new (ptr + i) T(value);
            }
        } else {
            size_ = 0;
        }
    }

    constexpr box(const box& other)
        requires std::is_copy_constructible_v<T>
                 and (not std::is_polymorphic_v<T>)
        : ptr(nullptr)
        , size_(other.size_)
        , alloc(other.alloc)
    {
        if (size_ == 0) {
            return;
        }
        ptr = glox::alloc_uninitialized<T>(alloc, size_).ptr;
        if (ptr != nullptr) {
            for (std::size_t i = 0; i < size_; ++i) {
                new (ptr + i) T(other.ptr[i]);
            }
        } else {
            size_ = 0;
        }
    }
    constexpr box& operator=(const box& other)
        requires std::is_copy_assignable_v<T> and (not std::is_polymorphic_v<T>)
    {
        using glox::swap;
        box tmp(other);
        swap(*this, tmp);

        return *this;
    }

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

template <typename T, glox::allocator<T> Allocator = glox::default_allocator<T>>
class [[clang::trivial_abi]] polybox
{
    static_assert(
        !std::is_array_v<T>,
        "polybox does not support array types T[]"
    );

    template <typename U, typename SrcAlloc = Allocator>
    static constexpr auto rebind_alloc(const SrcAlloc& a) noexcept
    {
        using ReboundAlloc = typename SrcAlloc::template rebind<U>::other;
        if constexpr (std::constructible_from<ReboundAlloc, const SrcAlloc&>) {
            return ReboundAlloc(a);
        } else {
            return ReboundAlloc { };
        }
    }

    template <typename U, typename SrcAlloc = Allocator>
    static constexpr auto rebind_alloc(SrcAlloc&& a) noexcept
        requires(!std::is_lvalue_reference_v<SrcAlloc>)
    {
        using ReboundAlloc = typename SrcAlloc::template rebind<U>::other;
        if constexpr (std::constructible_from<ReboundAlloc, SrcAlloc&&>) {
            return ReboundAlloc(RVALUE(a));
        } else {
            return ReboundAlloc { };
        }
    }
    struct control_ops
    {
        T* (*clone)(const T* src, Allocator& alloc);
        void (*destroy_and_dealloc)(T* ptr, Allocator& alloc) noexcept;
    };

    template <typename U>
    static constexpr control_ops OPS_FOR = {
        .clone = []() -> T* (*)(const T*, Allocator&) {
            if constexpr (std::is_copy_constructible_v<U>) {
                return +[](const T* src, Allocator& alloc) -> T* {
                    auto rebound = rebind_alloc<U>(alloc);
                    U* mem = glox::alloc_uninitialized<U>(rebound, 1).ptr;
                    if (mem != nullptr) {
                        std::construct_at(mem, *static_cast<const U*>(src));
                    }
                    return static_cast<T*>(mem);
                };
            } else {
                return nullptr;
            }
        }(),
        .destroy_and_dealloc = +[](T* ptr, Allocator& alloc) noexcept {
            auto rebound = rebind_alloc<U>(alloc);
            U* derivedPtr = static_cast<U*>(ptr);
            derivedPtr->U::~U();
            glox::dealloc(rebound, derivedPtr, 1);
        }
    };

    T* ptr;
    const control_ops* ops;
    [[no_unique_address]] Allocator alloc;

    template <typename U, glox::allocator<U> OtherAlloc>
    friend class polybox;

public:
    using allocator = Allocator;
    using element_type = T;
    using reference = T&;
    using pointer = T*;

    constexpr polybox() noexcept
        : ptr(nullptr)
        , ops(nullptr)
        , alloc(Allocator { })
    {
    }

    // In-place construction of concrete type T
    template <typename... Args>
        requires(!std::is_abstract_v<T>) && std::constructible_from<T, Args...>
    constexpr explicit polybox(in_place_t, Args&&... args)
        : ptr(glox::alloc_uninitialized<T>(alloc, 1).ptr)
        , ops(&OPS_FOR<T>)
        , alloc(Allocator { })
    {
        if (ptr != nullptr) {
            std::construct_at(ptr, FORWARD(args)...);
        } else {
            ops = nullptr;
        }
    }

    // In-place construction of derived type U
    template <typename U, typename... Args>
        requires std::derived_from<U, T> && (!std::is_abstract_v<U>)
                  && std::constructible_from<U, Args...>
    constexpr explicit polybox(in_place_type_t<U>, Args&&... args)
        : ptr(nullptr)
        , ops(&OPS_FOR<U>)
        , alloc(Allocator { })
    {
        auto rebound = rebind_alloc<U>(alloc);
        U* mem = glox::alloc_uninitialized<U>(rebound, 1).ptr;
        if (mem != nullptr) {
            std::construct_at(mem, FORWARD(args)...);
            ptr = static_cast<T*>(mem);
        } else {
            ops = nullptr;
        }
    }

    // Direct construction from a concrete/derived value (e.g.
    // polybox<Base>(Derived{...}))
    template <typename U>
        requires(!std::same_as<std::remove_cvref_t<U>, polybox>)
             && (!std::same_as<std::remove_cvref_t<U>, in_place_t>)
             && std::derived_from<std::remove_cvref_t<U>, T>
             && std::constructible_from<std::remove_cvref_t<U>, U>
    constexpr polybox(U&& value)
        : polybox(in_place_type<std::remove_cvref_t<U>>, FORWARD(value))
    {
    }

    // Deep polymorphic copy constructor via tracked concrete copy constructor
    constexpr polybox(const polybox& other)
        : ptr(nullptr)
        , ops(other.ops)
        , alloc(other.alloc)
    {
        if (other.ptr != nullptr && other.ops != nullptr) {
            GLOX_ASSERT(
                other.ops->clone != nullptr
                && "Stored dynamic type is not copy constructible"
            );
            ptr = other.ops->clone(other.ptr, alloc);
            if (ptr == nullptr) {
                ops = nullptr;
            }
        }
    }

    constexpr polybox& operator=(const polybox& other)
    {
        if (this != &other) {
            using glox::swap;
            polybox tmp(other);
            swap(ptr, tmp.ptr);
            swap(ops, tmp.ops);
            swap(alloc, tmp.alloc);
        }
        return *this;
    }

    constexpr polybox(polybox&& other) noexcept
        : ptr(glox::exchange(other.ptr, nullptr))
        , ops(glox::exchange(other.ops, nullptr))
        , alloc(RVALUE(other.alloc))
    {
    }

    template <typename U, typename OtherAlloc>
        requires(!std::same_as<U, T>) && std::derived_from<U, T>
    constexpr polybox(polybox<U, OtherAlloc>&& other) noexcept
        : ptr(nullptr)
        , ops(nullptr)
        , alloc(rebind_alloc<T>(RVALUE(other.alloc)))
    {
        if (other.ptr != nullptr) {
            ptr = static_cast<T*>(glox::exchange(other.ptr, nullptr));
            ops = &OPS_FOR<U>;
            other.ops = nullptr;
        }
    }
    constexpr polybox& operator=(polybox&& other) noexcept
    {
        if (this != &other) {
            using glox::swap;
            swap(ptr, other.ptr);
            swap(ops, other.ops);
            swap(alloc, other.alloc);
        }
        return *this;
    }

    constexpr ~polybox()
    {
        if (ptr != nullptr && ops != nullptr) {
            ops->destroy_and_dealloc(ptr, alloc);
            ptr = nullptr;
            ops = nullptr;
        }
    }

    // Factory helper for derived types: polybox<Base>::make<Derived>(args...)
    template <typename U = T, typename... Args>
        requires std::derived_from<U, T> && (!std::is_abstract_v<U>)
              && std::constructible_from<U, Args...>
    constexpr static glox::option<polybox> make(Args&&... args)
    {
        polybox val(in_place_type<U>, FORWARD(args)...);
        if (val.is_null()) {
            return { };
        }
        return val;
    }

    // Factory helper matching box::create
    constexpr static glox::option<polybox> create(auto&&... args)
    {
        glox::option<polybox> val(FORWARD(args)...);
        if (val.val().is_null()) {
            return { };
        }
        return val;
    }

    [[nodiscard]] constexpr bool is_null() const noexcept
    {
        return ptr == nullptr;
    }

    [[nodiscard]] constexpr bool is_copyable() const noexcept
    {
        return ptr == nullptr || (ops != nullptr && ops->clone != nullptr);
    }

    [[nodiscard]] constexpr auto get_allocator() const
    {
        return alloc;
    }

    [[nodiscard]] constexpr glox::option<T&> get() const noexcept
    {
        if (ptr != nullptr) {
            return ptr;
        }
        return { };
    }

    [[nodiscard]] constexpr T* get_ptr() const noexcept
    {
        return ptr;
    }

    [[nodiscard]] constexpr std::add_lvalue_reference_t<T>
    operator*() const noexcept
    {
        GLOX_ASSERT(ptr != nullptr);
        return *ptr;
    }

    [[nodiscard]] constexpr pointer operator->() const noexcept
    {
        return ptr;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return ptr != nullptr;
    }
};

template <typename T>
polybox(T) -> polybox<std::remove_reference_t<T>>;

template <typename T>
box(T) -> box<std::remove_reference_t<T>>;
GLOX_END_EXPORT
} // namespace glox
