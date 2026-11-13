#pragma once

#ifndef USE_MODULES
#include "alloc.hpp"
#include "assert.hpp"
#include "detail/memory.hpp"
#include "detail/moveutils.hpp"
#include "macros.hpp"
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
    struct callable_base
    {
        callable_base() = default;
        callable_base(const callable_base&) = delete;
        callable_base& operator=(const callable_base&) = delete;
        virtual ~callable_base() = default;

        virtual callable_base* clone() const = 0;
        virtual void clone_into(callable_base* dst) const = 0;
        virtual void move_into(callable_base* dst) noexcept = 0;
        virtual void destroy() noexcept = 0;
        virtual void destroy_deallocate() noexcept = 0;
        virtual R operator()(Args&&... args) = 0;
        virtual bool is_copyable() const noexcept = 0;
    };

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

        void clone_into(callable_base* dst) const override
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

        void move_into(callable_base* dst) noexcept override
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

        R operator()(Args&&... args) override
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
    alignas(std::max_align_t) unsigned char sbo_[SBO_SIZE];
    callable_base* ptr_ = nullptr;
    [[no_unique_address]] Allocator alloc_ { };

    template <typename Impl, typename F>
    static constexpr bool FITS_IN_SBO_V
        = sizeof(Impl) <= SBO_SIZE && alignof(Impl) <= alignof(std::max_align_t)
       && (std::is_move_constructible_v<F> || std::is_copy_constructible_v<F>);

    bool is_local() const noexcept
    {
        const auto p = reinterpret_cast<std::uintptr_t>(ptr_);
        const auto base = reinterpret_cast<std::uintptr_t>(sbo_);
        return p >= base && p < base + SBO_SIZE;
    }

    callable_base* local_ptr() noexcept
    {
        return std::launder(reinterpret_cast<callable_base*>(sbo_));
    }

    void reset() noexcept
    {
        if (ptr_ != nullptr) {
            if (is_local()) {
                ptr_->destroy();
            } else {
                ptr_->destroy_deallocate();
            }
            ptr_ = nullptr;
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
            ptr_ = local;
        } else {
            auto rebound = rebind_impl<Impl>(alloc_);
            Impl* mem = glox::alloc_uninitialized<Impl>(rebound, 1).ptr;
            if (mem == nullptr) {
                return;
            }
            std::construct_at(mem, FORWARD(callable), alloc_);
            ptr_ = mem;
        }
    }

    function(const function& other)
        : ptr_(nullptr)
        , alloc_(other.alloc_)
    {
        if (other.ptr_ != nullptr) {
            if (other.is_local()) {
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
        , alloc_(RVALUE(other.alloc_))
    {
        if (other.ptr_ != nullptr) {
            if (other.is_local()) {
                other.ptr_->move_into(local_ptr());
                other.ptr_->destroy();
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
            alloc_ = RVALUE(other.alloc_);
            if (other.ptr_ != nullptr) {
                if (other.is_local()) {
                    other.ptr_->move_into(local_ptr());
                    other.ptr_->destroy();
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
        return ptr_ == nullptr || ptr_->is_copyable();
    }

    [[nodiscard]] bool is_sbo() const noexcept
    {
        return ptr_ != nullptr && is_local();
    }

    R operator()(Args... args) const
    {
        GLOX_ASSERT(ptr_ != nullptr);
        if constexpr (std::is_void_v<R>) {
            (*ptr_)(FORWARD(args)...);
        } else {
            return (*ptr_)(FORWARD(args)...);
        }
    }
};

GLOX_END_EXPORT
} // namespace glox
