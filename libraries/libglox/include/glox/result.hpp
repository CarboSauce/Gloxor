#pragma once
#include "assert.hpp"
#include "detail/memory.hpp"
#include "glox/detail/try.hpp"
#include "macros.hpp"
#include "metaprog.hpp"
#include <type_traits>
namespace glox {

template <typename R>
struct result_traits;

template <typename R>
concept result_type = requires {
    typename result_traits<std::remove_cvref_t<R>>::value_type;
    typename result_traits<std::remove_cvref_t<R>>::error_type;
};

template <typename E>
struct error
{
    E err;
};

struct error_inplace_t
{ };
inline constexpr auto error_inplace = error_inplace_t { };

template <typename T, typename E>
class [[nodiscard]] result
{
    union
    {
        T _val;
        E _err;
    };
    bool hasValue;

    template <typename ErrT>
    static constexpr bool is_error = false;
    template <typename Err>
    static constexpr bool is_error<glox::error<Err>> = true;

public:
    template <typename U = std::remove_cv_t<T>>
    GLOX_ALWAYS_INLINE constexpr explicit(
        not std::is_convertible_v<U, T>
    ) result(U&& val)
        requires(not std::is_same_v<std::remove_cvref_t<U>, in_place_t>)
                and (not std::
                        is_same_v<std::remove_cvref_t<U>, error_inplace_t>)
                and (std::is_constructible_v<U, T>) and (not is_error<U>)
        : _val { FORWARD(val) }
        , hasValue(true)
    {
    }
    template <typename... Args>
    GLOX_ALWAYS_INLINE constexpr explicit result(
        glox::in_place_t,
        Args&&... args
    )
        : _val { FORWARD(args)... }
        , hasValue(true)
    {
    }
    template <typename... Args>
    GLOX_ALWAYS_INLINE constexpr explicit result(
        glox::error_inplace_t,
        Args&&... args
    )
        : _err { FORWARD(args)... }
        , hasValue(false)
    {
    }
    template <typename G>
    GLOX_ALWAYS_INLINE constexpr explicit(
        not std::is_convertible_v<G, E>
    ) result(error<G>&& err)
        : _err { RVALUE(err.err) }
        , hasValue(false)
    {
    }
    template <typename G>
    GLOX_ALWAYS_INLINE constexpr explicit(
        not std::is_convertible_v<G, E>
    ) result(const error<G>& err)
        : _err { err.err }
        , hasValue(false)
    {
    }

    constexpr ~result()
        requires std::is_trivially_destructible_v<T>
                 and std::is_trivially_destructible_v<E>
    = default;
    constexpr ~result()
    {
        if (hasValue)
            _val.~T();
        else
            _err.~E();
    }

    constexpr result(const result&)
        requires std::is_trivially_copy_constructible_v<T>
                 and std::is_trivially_copy_constructible_v<E>
    = default;
    constexpr result(const result& other)
        : hasValue(other.hasValue)
    {
        if (other.hasValue)
            std::construct_at(&_val, other._val);
        else
            std::construct_at(&_err, other._err);
    }

    constexpr result(result&&)
        requires std::is_trivially_move_constructible_v<T>
                 and std::is_trivially_move_constructible_v<E>
    = default;
    constexpr result(result&& other)
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
        requires(not std::is_trivially_copy_assignable_v<T>
                    or not std::is_trivially_copy_assignable_v<E>)
            and std::is_destructible_v<T> and std::is_destructible_v<E>
            and std::is_copy_constructible_v<T>
            and std::is_copy_constructible_v<E>
    {
        if (hasValue) {
            if (other.hasValue) {
                _val = other._val;
            } else {
                std::destroy_at(&_val);
                std::construct_at(&_err, other._err);
            }
        } else {
            if (other.hasValue) {
                std::destroy_at(&_err);
                std::construct_at(&_val, other._val);
            } else {
                _err = other._err;
            }
        }
        hasValue = other.hasValue;
        return *this;
    }

    constexpr result& operator=(result&&) = delete;
    constexpr result& operator=(result&&)
        requires std::is_trivially_move_assignable_v<T>
                 and std::is_trivially_move_assignable_v<E>
    = default;
    constexpr result& operator=(result&& other)
        requires(not std::is_trivially_move_assignable_v<T>
                    or not std::is_trivially_move_assignable_v<E>)
            and std::is_destructible_v<T> and std::is_destructible_v<E>
            and std::is_move_constructible_v<T>
            and std::is_move_constructible_v<E>
    {
        if (hasValue) {
            if (other.hasValue) {
                _val = RVALUE(other._val);
            } else {
                std::destroy_at(&_val);
                std::construct_at(&_err, RVALUE(other._err));
            }
        } else {
            if (other.hasValue) {
                std::destroy_at(&_err);
                std::construct_at(&_val, RVALUE(other._val));
            } else {
                _err = RVALUE(other._err);
            }
        }
        hasValue = other.hasValue;
        return *this;
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
        gloxAssert(hasValue, "Can't unwrap an error");
        return _val;
    }
    constexpr T&& val() &&
    {
        gloxAssert(hasValue, "Can't unwrap an error");
        return RVALUE(_val);
    }
    constexpr const T& val() const&
    {
        gloxAssert(hasValue, "Can't unwrap an error");
        return _val;
    }
    constexpr const T&& val() const&&
    {
        gloxAssert(hasValue, "Can't unwrap an error");
        return RVALUE(_val);
    }

    constexpr E& err() &
    {
        gloxAssert(not hasValue, "Can't unwrap an error");
        return _err;
    }
    constexpr E&& err() &&
    {
        gloxAssert(not hasValue, "Can't unwrap an error");
        return RVALUE(_err);
    }
    constexpr const E& err() const&
    {
        gloxAssert(not hasValue, "Can't unwrap an error");
        return _err;
    }
    constexpr const E&& err() const&&
    {
        gloxAssert(not hasValue, "Can't unwrap an error");
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
            return result<G, E> { in_place, FORWARD(f)(FORWARD(self._val)) };
        else
            return result<G, E>(FORWARD(self));
    }

    template <typename Self, typename Func>
    constexpr auto transform_err(this Self&& self, Func&& f)
    {
        using G = decltype(FORWARD(f)(FORWARD(self._err)));
        if (not self.hasValue)
            return result<T, G>(error_inplace, FORWARD(f)(FORWARD(self._err)));
        else
            return result<T, G>(in_place, FORWARD(self));
    }

    template <result_type R>
    friend constexpr decltype(auto) try_propagate_err(R& res);
    template <result_type R>
    friend constexpr decltype(auto) try_propagate_val(R& res);
};

template <typename T, typename E>
struct result_traits<result<T, E>>
{
    using value_type = T;
    using error_type = E;
};

template <result_type R>
GLOX_ALWAYS_INLINE constexpr auto try_propagate_err(R&& res)
{
    return FORWARD(res).err();
}

template <typename T, typename E>
GLOX_ALWAYS_INLINE constexpr result<T, E> try_propagate_from_err(E&& res)
{
    return result<T, E> { error_inplace, FORWARD(res) };
}
} // namespace glox
