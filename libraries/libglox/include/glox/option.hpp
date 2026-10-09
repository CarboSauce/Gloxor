#pragma once
#include "assert.hpp"
#include "detail/memory.hpp"
#include "macros.hpp"
#include "metaprog.hpp"
#ifndef USE_MODULES
#include <type_traits>
#endif
namespace glox {
GLOX_BEGIN_EXPORT

struct empty_option_t
{ };

constexpr inline auto empty_option = empty_option_t { };

template <typename T>
class [[nodiscard]] option
{
    union
    {
        T _val;
    };
    bool hasValue;

public:
    template <typename U = std::remove_cv_t<T>>
    GLOX_ALWAYS_INLINE constexpr option(U&& val)
        requires(std::is_constructible_v<U, T>)
                and (not std::is_same_v<U, empty_option_t>)
        : _val { FORWARD(val) }
        , hasValue(true)
    {
    }
    template <typename... Args>
    GLOX_ALWAYS_INLINE constexpr explicit option(
        glox::in_place_t,
        Args&&... args
    )
        : _val { FORWARD(args)... }
        , hasValue(true)
    {
    }

    GLOX_ALWAYS_INLINE constexpr option()
        : hasValue(false)
    {
    }
    GLOX_ALWAYS_INLINE constexpr option(empty_option_t)
        : hasValue(false)
    {
    }

    constexpr ~option()
        requires std::is_trivially_destructible_v<T>
    = default;
    constexpr ~option()
    {
        if (hasValue)
            _val.~T();
    }

    constexpr option(const option&)
        requires std::is_trivially_copy_constructible_v<T>
    = default;
    constexpr option(const option& other)
        : hasValue(other.hasValue)
    {
        if (other.hasValue)
            std::construct_at(&_val, other._val);
    }

    constexpr option(option&&)
        requires std::is_trivially_move_constructible_v<T>
    = default;
    constexpr option(option&& other)
        : hasValue(other.hasValue)
    {
        if (other.hasValue)
            std::construct_at(&_val, RVALUE(other._val));
    }

    constexpr option& operator=(const option&) = delete;
    constexpr option& operator=(const option&)
        requires std::is_trivially_copy_assignable_v<T>
    = default;
    constexpr option& operator=(const option& other)
        requires(not std::is_trivially_copy_assignable_v<T>)
            and std::is_destructible_v<T> and std::is_copy_constructible_v<T>
    {
        if (hasValue) {
            if (other.hasValue) {
                _val = other._val;
            } else {
                std::destroy_at(_val);
            }
        } else {
            if (other.hasValue) {
                std::construct_at(_val, other._val);
            }
        }
        hasValue = other.hasValue;
    }

    constexpr option& operator=(option&&) = delete;
    constexpr option& operator=(option&&)
        requires std::is_trivially_move_assignable_v<T>
    = default;
    constexpr option& operator=(option&& other)
        requires(not std::is_trivially_move_assignable_v<T>)
            and std::is_destructible_v<T> and std::is_move_constructible_v<T>
    {
        if (hasValue) {
            if (other.hasValue) {
                _val = RVALUE(other._val);
            } else {
                std::destroy_at(_val);
            }
        } else {
            if (other.hasValue) {
                std::construct_at(_val, RVALUE(other._val));
            } else {
                std::destroy_at(_val);
            }
        }
    }

    GLOX_ALWAYS_INLINE
    static constexpr option from_val(T&& val)
    {
        return option { RVALUE(val) };
    }
    GLOX_ALWAYS_INLINE
    static constexpr option from_val(const T& val)
    {
        return option { val };
    }

    GLOX_ALWAYS_INLINE constexpr auto&& val(
        this auto&& self,
        std::source_location loc = std::source_location::current()
    )
    {
        GLOX_ASSERT(FORWARD(self).hasValue, "Can't unwrap an error", loc);
        return FORWARD(self)._val;
    }

    [[nodiscard]] constexpr bool has_val() const
    {
        return hasValue;
    }
    [[nodiscard]] constexpr operator bool() const
    {
        return static_cast<bool>(hasValue);
    }
    [[nodiscard]] constexpr auto operator<=>(const option& b)
    {
        if (hasValue and b.hasValue)
            return _val <=> b._val;
        else
            return false;
    }
    constexpr bool operator==(const option& b) const
    {
        if (hasValue and b.hasValue)
            return _val == b._val;
        else if (not hasValue and not b.hasValue)
            return true;
        else
            return false;
    }
    constexpr T unwrap_or(T&& def) &&
    {
        if (hasValue)
            return RVALUE(_val);
        return RVALUE(def);
    }

    template <typename U = std::remove_cv_t<T>>
    constexpr T val_or(U&& def) &&
    {
        if (hasValue) {
            return RVALUE(val());
        } else {
            return static_cast<T>(FORWARD(def));
        }
    }
    template <typename U = std::remove_cv_t<T>>
    constexpr T val_or(U&& def) const&
    {
        if (hasValue) {
            return val();
        } else {
            return static_cast<T>(FORWARD(def));
        }
    }

    template <typename Self, typename Func>
    constexpr auto and_then(this Self&& self, Func&& f)
    {
        if (self.hasValue)
            return FORWARD(f)(FORWARD(self)._val);
        else
            return FORWARD(self);
    }

    template <typename Self, typename Func>
    constexpr auto or_else(this Self&& self, Func&& f)
    {
        if (not self.hasValue)
            return FORWARD(f)();
        else
            return FORWARD(self);
    }

    template <typename Self, typename Func>
    constexpr auto transform(this Self&& self, Func&& f)
    {
        using G = decltype(FORWARD(f)(FORWARD(self)._val));
        if (self.hasValue)
            return option<G> { FORWARD(f)(FORWARD(self)._val) };
        else
            return FORWARD(self);
    }
};

template <typename T>
struct [[nodiscard]] option<T&>
{
    T* _val;

public:
    GLOX_ALWAYS_INLINE constexpr option(T& val)
        : _val { &val }
    {
    }

    GLOX_ALWAYS_INLINE constexpr option()
        : _val { nullptr }
    {
    }

    GLOX_ALWAYS_INLINE constexpr option(empty_option_t)
        : _val { nullptr }
    {
    }

    constexpr ~option() = default;
    constexpr option(const option&) = default;
    constexpr option(option&&) = default;
    constexpr option& operator=(const option&) = default;
    constexpr option& operator=(option&&) = default;

    GLOX_ALWAYS_INLINE
    static constexpr option from_val(T& val)
    {
        return option { val };
    }
    GLOX_ALWAYS_INLINE

    GLOX_ALWAYS_INLINE constexpr auto&& val(
        this auto&& self,
        std::source_location loc = std::source_location::current()
    )
    {
        GLOX_ASSERT(
            FORWARD(self)._val != nullptr, "Can't unwrap an error", loc
        );
        return *FORWARD(self)._val;
    }
    GLOX_ALWAYS_INLINE constexpr T& operator*(this auto&& self)
    {
        GLOX_ASSERT(FORWARD(self)._val != nullptr, "Can't unwrap an error");
        return *FORWARD(self)._val;
    }
    GLOX_ALWAYS_INLINE constexpr const T* operator->() const
    {
        GLOX_ASSERT(_val != nullptr, "Can't unwrap an error");
        return _val;
    }
    GLOX_ALWAYS_INLINE constexpr T* operator->()
    {
        GLOX_ASSERT(_val != nullptr, "Can't unwrap an error");
        return _val;
    }

    [[nodiscard]] constexpr bool has_val() const
    {
        return _val != nullptr;
    }
    [[nodiscard]] constexpr operator bool() const
    {
        return static_cast<bool>(_val != nullptr);
    }
    constexpr auto operator<=>(const option& b)
    {
        return _val <=> b._val;
    }
    constexpr bool operator==(const option& b) const
    {
        return _val == b;
    }

    template <typename U = std::remove_cv_t<T>>
    constexpr std::remove_cv_t<T> val_or(U&& def)
    {
        if (has_val()) {
            return *_val;
        } else {
            return static_cast<std::remove_cv_t<T>>(FORWARD(def));
        }
    }

    template <typename Self, typename Func>
    constexpr auto and_then(this Self&& self, Func&& f)
    {
        using G = decltype(FORWARD(f)(*FORWARD(self)._val));
        if (self.has_val())
            return FORWARD(f)(*self._val);
        else
            return std::remove_cv_t<G>();
    }

    template <typename Self, typename Func>
    constexpr auto or_else(this Self&& self, Func&& f)
    {
        if (self.has_val())
            return *self._val;
        else
            return FORWARD(f)();
    }

    template <typename Self, typename Func>
    constexpr auto transform(this Self&& self, Func&& f)
    {
        using G = decltype(FORWARD(f)(*FORWARD(self)._val));
        if (self.has_val())
            return option<G> { FORWARD(f)(*FORWARD(self)._val) };
        else
            return option<G> { };
    }
};

template <typename T>
option(T) -> option<T>;

template <typename T>
GLOX_ALWAYS_INLINE constexpr empty_option_t try_propagate_err(
    [[maybe_unused]] option<T>&& res
)
{
    return { };
}

template <typename T>
GLOX_ALWAYS_INLINE constexpr option<T> try_propagate_from_err(
    empty_option_t opt
)
{
    return opt;
}

GLOX_END_EXPORT
} // namespace glox
