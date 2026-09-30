#pragma once
#include "assert.hpp"
#include "detail/memory.hpp"
#include "macros.hpp"
#include "metaprog.hpp"
#include <type_traits>
namespace glox {

template <typename T>
class [[nodiscard]] option
{
    union
    {
        T _val;
    };
    bool hasValue;

public:
    GLOX_ALWAYS_INLINE constexpr option(T&& val)
        : _val { RVALUE(val) }
        , hasValue(true)
    {
    }
    GLOX_ALWAYS_INLINE constexpr option(const T& val)
        : _val { val }
        , hasValue(true)
    {
    }
    GLOX_ALWAYS_INLINE constexpr option()
        : hasValue(false)
    {
    }

    constexpr ~option() = default;
    constexpr ~option()
        requires(not std::is_trivially_destructible_v<T>)
    {
        if (hasValue)
            _val.~T();
    }

    constexpr option(const option&) = default;
    constexpr option(const option& other)
        requires(not std::is_trivially_constructible_v<T>)
        : hasValue(other.hasValue)
    {
        if (other.hasValue)
            std::construct_at(&_val, other._val);
    }

    constexpr option(option&&) = default;
    constexpr option(option&& other)
        requires(not std::is_trivially_move_constructible_v<T>)
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

    constexpr T& val() &
    {
        gloxAssert(not hasValue, "Can't unwrap an error");
        return _val;
    }
    constexpr T&& val() &&
    {
        gloxAssert(not hasValue, "Can't unwrap an error");
        return RVALUE(_val);
    }
    constexpr const T& val() const&
    {
        gloxAssert(not hasValue, "Can't unwrap an error");
        return _val;
    }
    constexpr const T&& val() const&&
    {
        gloxAssert(not hasValue, "Can't unwrap an error");
        return RVALUE(_val);
    }

    constexpr bool has_val() const
    {
        return hasValue;
    }
    constexpr operator bool() const
    {
        return static_cast<bool>(hasValue);
    }
    constexpr auto operator<=>(const option& b)
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
            return FORWARD(f)(FORWARD(self._val));
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
        using G = decltype(FORWARD(f)(FORWARD(self._val)));
        if (self.hasValue)
            return option<G> { FORWARD(f)(FORWARD(self._val)) };
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

    constexpr T& val() const&
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return *_val;
    }
    constexpr T& val() &
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return *_val;
    }
    constexpr const T&& val() const&&
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return RVALUE(*_val);
    }
    constexpr T&& val() &&
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return RVALUE(*_val);
    }
    constexpr T& operator*() const&
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return *_val;
    }
    constexpr T& operator*() &
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return *_val;
    }
    constexpr const T&& operator*() const&&
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return RVALUE(*_val);
    }
    constexpr T&& operator*() &&
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return RVALUE(*_val);
    }
    constexpr const T* operator->() const
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return _val;
    }
    constexpr T* operator->()
    {
        gloxAssert(_val != nullptr, "Can't unwrap an error");
        return _val;
    }

    constexpr bool has_val() const
    {
        return _val != nullptr;
    }
    constexpr operator bool() const
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
        using G = decltype(FORWARD(f)(*self._val));
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
        using G = decltype(FORWARD(f)(*self._val));
        if (self.has_val())
            return option<G> { FORWARD(f)(*self._val) };
        else
            return option<G> { };
    }
};

template <typename T>
option(T) -> option<T>;
} // namespace glox
