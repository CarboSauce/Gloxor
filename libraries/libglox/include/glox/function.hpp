#pragma once

#ifndef USE_MODULES
#include "alloc.hpp"
#include "assert.hpp"
#include "detail/memory.hpp"
#include "detail/moveutils.hpp"
#include "macros.hpp"
#include "string.h"
#include "util.hpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#endif

namespace glox {
GLOX_BEGIN_EXPORT

template <typename Sig>
class function_ref;

template <typename R, typename... Args>
class [[clang::trivial_abi]] function_ref<R(Args...)>
{
    union storage_t
    {
        void* obj;
        const void* const_obj;
        void (*fn_ptr)();
    };

    using invoker_t = R (*)(storage_t, Args&&...);

    storage_t storage_ { };
    invoker_t invoke_ = nullptr;

    template <typename F>
    static constexpr invoker_t make_invoker() noexcept
    {
        if constexpr (
            std::is_pointer_v<F> && std::is_function_v<std::remove_pointer_t<F>>
        ) {
            return +[](storage_t s, Args&&... args) -> R {
                auto fn = reinterpret_cast<F>(s.fn_ptr);
                if constexpr (std::is_void_v<R>) {
                    fn(FORWARD(args)...);
                } else {
                    return fn(FORWARD(args)...);
                }
            };
        } else if constexpr (std::is_const_v<F>) {
            return +[](storage_t s, Args&&... args) -> R {
                const auto& obj
                    = *static_cast<const std::remove_const_t<F>*>(s.const_obj);
                if constexpr (std::is_void_v<R>) {
                    obj(FORWARD(args)...);
                } else {
                    return obj(FORWARD(args)...);
                }
            };
        } else {
            return +[](storage_t s, Args&&... args) -> R {
                auto& obj = *static_cast<F*>(s.obj);
                if constexpr (std::is_void_v<R>) {
                    obj(FORWARD(args)...);
                } else {
                    return obj(FORWARD(args)...);
                }
            };
        }
    }

public:
    constexpr function_ref() noexcept = default;
    constexpr function_ref(std::nullptr_t) noexcept { }

    constexpr function_ref(const function_ref&) noexcept = default;
    constexpr function_ref& operator=(const function_ref&) noexcept = default;

    template <typename F>
        requires std::is_function_v<F> && std::is_invocable_r_v<R, F*, Args...>
    constexpr function_ref(F* fn) noexcept
    {
        if (fn != nullptr) {
            storage_.fn_ptr = reinterpret_cast<void (*)()>(fn);
            invoke_ = make_invoker<F*>();
        }
    }

    template <typename F>
        requires(!std::same_as<std::remove_cvref_t<F>, function_ref>)
             && (!std::is_pointer_v<std::remove_reference_t<F>>)
             && std::is_invocable_r_v<R, F&, Args...>
    constexpr function_ref(F&& callable) noexcept
    {
        using RawF = std::remove_reference_t<F>;
        if constexpr (std::is_const_v<RawF>) {
            storage_.const_obj
                = static_cast<const void*>(std::addressof(callable));
        } else {
            storage_.obj = static_cast<void*>(std::addressof(callable));
        }
        invoke_ = make_invoker<RawF>();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return invoke_ != nullptr;
    }

    constexpr R operator()(Args... args) const
    {
        GLOX_ASSERT(invoke_ != nullptr);
        if constexpr (std::is_void_v<R>) {
            invoke_(storage_, FORWARD(args)...);
        } else {
            return invoke_(storage_, FORWARD(args)...);
        }
    }
};

template <
    typename Sig,
    typename Allocator = glox::default_allocator<unsigned char>
>
class function;

template <typename R, typename... Args, typename Allocator>
class function<R(Args...), Allocator>
{
    template <typename T>
    using fast_param_t = std::conditional_t<
        std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(void*) * 2,
        T,
        T&&
    >;

    struct callable_base
    {
        callable_base() = default;
        callable_base(const callable_base&) = delete;
        callable_base& operator=(const callable_base&) = delete;

        // Slot 0: [vptr + 0]
        virtual R operator()(fast_param_t<Args>... args) = 0;
        virtual callable_base* clone() const = 0;
        virtual void clone_into(void* dst) const = 0;
        virtual void move_and_destroy(void* dst) noexcept = 0;
        virtual void destroy() noexcept = 0;
        virtual void destroy_deallocate() noexcept = 0;
        virtual bool is_copyable() const noexcept = 0;
        virtual ~callable_base() = default;
    };

    using invoker_t = R (*)(callable_base*, fast_param_t<Args>...);

    template <typename F>
    struct callable_impl final : callable_base
    {
        F fn_;
        [[no_unique_address]] Allocator alloc_;

        template <typename FnArg>
        explicit callable_impl(FnArg&& fn, const Allocator& alloc)
            : fn_(FORWARD(fn))
            , alloc_(alloc)
        {
        }

        callable_base* clone() const override
        {
            if constexpr (std::is_copy_constructible_v<F>) {
                using Rebound =
                    typename Allocator::template rebind<callable_impl>::other;
                Rebound rebound = rebind_impl<callable_impl>(alloc_);
                callable_impl* mem
                    = glox::alloc_uninitialized<callable_impl>(rebound, 1).ptr;
                GLOX_ASSERT(mem != nullptr);
                std::construct_at(mem, fn_, alloc_);
                return mem;
            } else {
                GLOX_ASSERT(
                    false && "Stored callable is not copy constructible"
                );
                return nullptr;
            }
        }

        void clone_into(void* dst) const override
        {
            if constexpr (std::is_copy_constructible_v<F>) {
                std::construct_at(
                    static_cast<callable_impl*>(dst), fn_, alloc_
                );
            } else {
                GLOX_ASSERT(
                    false && "Stored callable is not copy constructible"
                );
            }
        }

        void move_and_destroy(void* dst) noexcept override
        {
            if constexpr (std::is_move_constructible_v<F>) {
                std::construct_at(
                    static_cast<callable_impl*>(dst),
                    RVALUE(fn_),
                    RVALUE(alloc_)
                );
            } else {
                std::construct_at(
                    static_cast<callable_impl*>(dst), fn_, RVALUE(alloc_)
                );
            }
            this->~callable_impl();
        }

        void destroy() noexcept override
        {
            this->~callable_impl();
        }

        void destroy_deallocate() noexcept override
        {
            using Rebound =
                typename Allocator::template rebind<callable_impl>::other;
            Rebound rebound = rebind_impl<callable_impl>(alloc_);
            this->~callable_impl();
            glox::dealloc(rebound, this, 1);
        }

        R operator()(fast_param_t<Args>... args) override
        {
            if constexpr (std::is_void_v<R>) {
                fn_(FORWARD(args)...);
            } else {
                return fn_(FORWARD(args)...);
            }
        }

        bool is_copyable() const noexcept override
        {
            return std::is_copy_constructible_v<F>;
        }
    };

    template <typename F>
    static constexpr invoker_t make_invoker() noexcept
    {
        return +[](callable_base* base, fast_param_t<Args>... args) -> R {
            auto& fn = static_cast<callable_impl<F>*>(base)->fn_;
            if constexpr (std::is_void_v<R>) {
                fn(FORWARD(args)...);
            } else {
                return fn(FORWARD(args)...);
            }
        };
    }

    template <typename Target>
    static constexpr auto rebind_impl(const Allocator& a) noexcept
    {
        using Rebound = typename Allocator::template rebind<Target>::other;
        if constexpr (std::constructible_from<Rebound, const Allocator&>) {
            return Rebound(a);
        } else {
            return Rebound { };
        }
    }

    static constexpr std::size_t SBO_SIZE = sizeof(void*) * 3;
    static constexpr std::uintptr_t TRIVIAL_TAG = 1;

    callable_base* ptr_ = nullptr;
    invoker_t invoker_ = nullptr;
    alignas(std::max_align_t) unsigned char sbo_[SBO_SIZE];
    [[no_unique_address]] Allocator alloc_ { };

    template <typename Impl, typename F>
    static constexpr bool FITS_IN_SBO_V
        = sizeof(Impl) <= SBO_SIZE && alignof(Impl) <= alignof(std::max_align_t)
       && (std::is_move_constructible_v<F> || std::is_copy_constructible_v<F>);

    template <typename F>
    static constexpr bool IS_TRIVIAL_SBO_V
        = std::is_trivially_copyable_v<F> && std::is_trivially_destructible_v<F>
       && std::is_trivially_copyable_v<Allocator>
       && std::is_trivially_destructible_v<Allocator>;

    bool is_trivial_local() const noexcept
    {
        return (reinterpret_cast<std::uintptr_t>(ptr_) & TRIVIAL_TAG) != 0;
    }

    callable_base* untagged_ptr() const noexcept
    {
        return reinterpret_cast<callable_base*>(
            reinterpret_cast<std::uintptr_t>(ptr_) & ~TRIVIAL_TAG
        );
    }

    bool is_local() const noexcept
    {
        if (is_trivial_local()) {
            return true;
        }
        const auto p = reinterpret_cast<std::uintptr_t>(ptr_);
        const auto base = reinterpret_cast<std::uintptr_t>(sbo_);
        return p >= base && p < base + SBO_SIZE;
    }

    callable_base* local_ptr() noexcept
    {
        return std::launder(reinterpret_cast<callable_base*>(sbo_));
    }

    callable_base* tagged_local_ptr() noexcept
    {
        return reinterpret_cast<callable_base*>(
            reinterpret_cast<std::uintptr_t>(sbo_) | TRIVIAL_TAG
        );
    }

    void reset() noexcept
    {
        if (ptr_ != nullptr) {
            if (!is_trivial_local()) {
                if (is_local()) {
                    ptr_->destroy();
                } else {
                    ptr_->destroy_deallocate();
                }
            }
            ptr_ = nullptr;
            invoker_ = nullptr;
        }
    }

public:
    using result_type = R;
    using allocator_type = Allocator;

    constexpr function() noexcept = default;
    constexpr function(std::nullptr_t) noexcept { }

    template <typename F>
        requires(!std::same_as<std::remove_cvref_t<F>, function>)
                 && std::is_invocable_r_v<R, std::decay_t<F>&, Args...>
    function(F&& callable)
        : ptr_(nullptr)
        , invoker_(nullptr)
        , alloc_(Allocator { })
    {
        using DecayedF = std::decay_t<F>;
        using Impl = callable_impl<DecayedF>;

        if constexpr (std::is_pointer_v<DecayedF>) {
            if (callable == nullptr) {
                return;
            }
        }

        if constexpr (FITS_IN_SBO_V<Impl, DecayedF>) {
            Impl* local = std::construct_at(
                reinterpret_cast<Impl*>(sbo_), FORWARD(callable), alloc_
            );
            if constexpr (IS_TRIVIAL_SBO_V<DecayedF>) {
                ptr_ = tagged_local_ptr();
            } else {
                ptr_ = local;
            }
        } else {
            auto rebound = rebind_impl<Impl>(alloc_);
            Impl* mem = glox::alloc_uninitialized<Impl>(rebound, 1).ptr;
            if (mem == nullptr) {
                return;
            }
            std::construct_at(mem, FORWARD(callable), alloc_);
            ptr_ = mem;
        }

        invoker_ = make_invoker<DecayedF>();
    }

    function(const function& other)
        : ptr_(nullptr)
        , invoker_(other.invoker_)
        , alloc_(other.alloc_)
    {
        if (other.ptr_ != nullptr) {
            if (other.is_trivial_local()) {
                glox::memcpy(sbo_, other.sbo_, SBO_SIZE);
                ptr_ = tagged_local_ptr();
            } else if (other.is_local()) {
                other.ptr_->clone_into(local_ptr());
                ptr_ = local_ptr();
            } else {
                ptr_ = other.ptr_->clone();
            }
        }
    }

    function& operator=(const function& other)
    {
        if (this != &other) {
            function tmp(other);
            *this = RVALUE(tmp);
        }
        return *this;
    }

    function(function&& other) noexcept
        : ptr_(nullptr)
        , invoker_(glox::exchange(other.invoker_, nullptr))
        , alloc_(RVALUE(other.alloc_))
    {
        if (other.ptr_ != nullptr) {
            if (other.is_trivial_local()) {
                glox::memcpy(sbo_, other.sbo_, SBO_SIZE);
                other.ptr_ = nullptr;
                ptr_ = tagged_local_ptr();
            } else if (other.is_local()) {
                other.ptr_->move_and_destroy(local_ptr());
                other.ptr_ = nullptr;
                ptr_ = local_ptr();
            } else {
                ptr_ = glox::exchange(other.ptr_, nullptr);
            }
        }
    }

    function& operator=(function&& other) noexcept
    {
        if (this != &other) {
            reset();
            invoker_ = glox::exchange(other.invoker_, nullptr);
            alloc_ = RVALUE(other.alloc_);
            if (other.ptr_ != nullptr) {
                if (other.is_trivial_local()) {
                    glox::memcpy(sbo_, other.sbo_, SBO_SIZE);
                    other.ptr_ = nullptr;
                    ptr_ = tagged_local_ptr();
                } else if (other.is_local()) {
                    other.ptr_->move_and_destroy(local_ptr());
                    other.ptr_ = nullptr;
                    ptr_ = local_ptr();
                } else {
                    ptr_ = glox::exchange(other.ptr_, nullptr);
                }
            }
        }
        return *this;
    }

    function& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    ~function()
    {
        reset();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return ptr_ != nullptr;
    }

    [[nodiscard]] bool is_copyable() const noexcept
    {
        return ptr_ == nullptr || is_trivial_local() || ptr_->is_copyable();
    }

    [[nodiscard]] bool is_sbo() const noexcept
    {
        return ptr_ != nullptr && is_local();
    }

    R operator()(Args... args) const
    {
        GLOX_ASSERT(invoker_ != nullptr);
        if constexpr (std::is_void_v<R>) {
            invoker_(untagged_ptr(), static_cast<fast_param_t<Args>>(args)...);
        } else {
            return invoker_(
                untagged_ptr(), static_cast<fast_param_t<Args>>(args)...
            );
        }
    }
};
GLOX_END_EXPORT
} // namespace glox
