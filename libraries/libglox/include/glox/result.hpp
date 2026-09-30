#pragma once
#include "assert.hpp"
#include "detail/memory.hpp"
#include "macros.hpp"
#include "metaprog.hpp"
#include <type_traits>
namespace glox {

template <typename T, typename E>
class [[nodiscard]] result
{
    constexpr static bool IS_CONV = std::is_convertible<T, E>::value;
    union
    {
        T _val;
        E _err;
    };
    bool hasValue;

    struct error_ref
    {
        const E& err;
    };

    struct error_mv
    {
        E&& err;
    };

    GLOX_ALWAYS_INLINE constexpr explicit(IS_CONV) result(const error_ref& err)
        : _err { err.err }
        , hasValue(false)
    {
    }
    GLOX_ALWAYS_INLINE constexpr explicit(IS_CONV) result(error_mv&& err)
        : _err { RVALUE(err.err) }
        , hasValue(false)
    {
    }

public:
    GLOX_ALWAYS_INLINE constexpr explicit(IS_CONV) result(T&& val)
        : _val { RVALUE(val) }
        , hasValue(true)
    {
    }
    GLOX_ALWAYS_INLINE constexpr explicit(IS_CONV) result(const T& val)
        : _val { val }
        , hasValue(true)
    {
    }
    GLOX_ALWAYS_INLINE constexpr explicit(IS_CONV) result(const E& err)
        requires(not IS_CONV)
        : _err { err }
        , hasValue(false)
    {
    }
    GLOX_ALWAYS_INLINE constexpr explicit(IS_CONV) result(E&& err)
        requires(not IS_CONV)
        : _err { RVALUE(err) }
        , hasValue(false)
    {
    }

    constexpr ~result() = default;
    constexpr ~result()
        requires(not std::is_trivially_destructible_v<T>)
            and (not std::is_trivially_destructible_v<E>)
    {
        if (hasValue)
            _val.~T();
        else
            _err.~T();
    }

    constexpr result(const result&) = default;
    constexpr result(const result& other)
        requires(not std::is_trivially_constructible_v<T>)
            and (not std::is_trivially_constructible_v<E>)
        : hasValue(other.hasValue)
    {
        if (other.hasValue)
            std::construct_at(&_val, other._val);
        else
            std::construct_at(&_err, other._err);
    }

    constexpr result(result&&) = default;
    constexpr result(result&& other)
        requires(not std::is_trivially_move_constructible_v<T>)
            and (not std::is_trivially_move_constructible_v<E>)
        : hasValue(other.hasValue)
    {
        if (other.hasValue)
            std::construct_at(&_val, RVALUE(other._val));
        else
            std::construct_at(&_err, RVALUE(other._err));
    }

    constexpr result& operator=(const result&) = delete;
    constexpr result& operator=(const result&)
        requires std::is_trivially_copy_assignable_v<T>
                 and std::is_trivially_copy_assignable_v<E>
    = default;
    constexpr result& operator=(const result& other)
        requires(not std::is_trivially_copy_assignable_v<T>)
            and (not std::is_trivially_copy_assignable_v<E>)
            and std::is_destructible_v<T> and std::is_destructible_v<E>
            and std::is_copy_constructible_v<T>
            and std::is_copy_constructible_v<E>
    {
        if (hasValue) {
            if (other.hasValue) {
                _val = other._val;
            } else {
                std::destroy_at(_val);
                std::construct_at(_err, other._err);
            }
        } else {
            if (other.hasValue) {
                std::destroy_at(_err);
                std::construct_at(_val, other._val);
            } else {
                _err = other._err;
            }
        }
        hasValue = other.hasValue;
    }

    constexpr result& operator=(result&&) = delete;
    constexpr result& operator=(result&&)
        requires std::is_trivially_move_assignable_v<T>
                 and std::is_trivially_move_assignable_v<E>
    = default;
    constexpr result& operator=(result&& other)
        requires(not std::is_trivially_move_assignable_v<T>)
            and (not std::is_trivially_move_assignable_v<E>)
            and std::is_destructible_v<T> and std::is_destructible_v<E>
            and std::is_move_constructible_v<T>
            and std::is_move_constructible_v<E>
    {
        if (hasValue) {
            if (other.hasValue) {
                _val = RVALUE(other._val);
            } else {
                std::destroy_at(_val);
                std::construct_at(_err, RVALUE(other._err));
            }
        } else {
            if (other.hasValue) {
                std::destroy_at(_err);
                std::construct_at(_val, RVALUE(other._val));
            } else {
                _err = RVALUE(other._err);
            }
        }
        hasValue = other.hasValue;
    }

    GLOX_ALWAYS_INLINE
    static constexpr result from_err(E&& err)
    {
        return result { result::error_mv { FORWARD(err) } };
    }
    GLOX_ALWAYS_INLINE
    static constexpr result from_err(const E& err)
    {
        return result { result::error_ref { err } };
    }
    GLOX_ALWAYS_INLINE
    static constexpr result from_val(T&& val)
    {
        return result { RVALUE(val) };
    }
    GLOX_ALWAYS_INLINE
    static constexpr result from_val(const T& val)
    {
        return result { val };
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

    constexpr E& err() &
    {
        gloxAssert(hasValue, "Can't unwrap an error");
        return _err;
    }
    constexpr E&& err() &&
    {
        gloxAssert(hasValue, "Can't unwrap an error");
        return RVALUE(_err);
    }
    constexpr const E& err() const&
    {
        gloxAssert(hasValue, "Can't unwrap an error");
        return _err;
    }
    constexpr const E&& err() const&&
    {
        gloxAssert(hasValue, "Can't unwrap an error");
        return RVALUE(_err);
    }

    constexpr bool has_val() const
    {
        return hasValue;
    }
    constexpr bool is_err() const
    {
        return not hasValue;
    }
    constexpr operator bool() const
    {
        return static_cast<bool>(hasValue);
    }
    constexpr auto operator<=>(const result& b)
    {
        if (hasValue and b.hasValue)
            return _val <=> b._val;
        else if (not hasValue and not b.hasValue)
            return _err <=> b._err;
        else
            return false;
    }
    constexpr bool operator==(const result& b) const
    {
        if (hasValue and b.hasValue)
            return _val == b._val;
        else if (not hasValue and not b.hasValue)
            return _err == b._err;
        else
            return false;
    }
    constexpr bool operator==(const E& b) const
    {
        if (hasValue)
            return false;
        else
            return _err == b._err;
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

    template <typename U = E>
    constexpr E err_or(U&& def) &&
    {
        if (hasValue) {
            return FORWARD(def);
        } else {
            return RVALUE(err());
        }
    }
    template <typename U = E>
    constexpr E err_or(U&& def) const&
    {
        if (hasValue) {
            return FORWARD(def);
        } else {
            return err();
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
            return FORWARD(f)(FORWARD(self._err));
        else
            return FORWARD(self);
    }

    template <typename Self, typename Func>
    constexpr auto transform(this Self&& self, Func&& f)
    {
        using G = decltype(FORWARD(f)(FORWARD(self._val)));
        if (self.hasValue)
            return result<G, E> { FORWARD(f)(FORWARD(self._val)) };
        else
            return result<G, E>(FORWARD(self));
    }

    template <typename Self, typename Func>
    constexpr auto transform_err(this Self&& self, Func&& f)
    {
        using G = decltype(FORWARD(f)(FORWARD(self._err)));
        if (not self.hasValue)
            return result<T, G>::from_err(FORWARD(f)(FORWARD(self._err)));
        else
            return result<T, G>(FORWARD(self));
    }
};
} // namespace glox
