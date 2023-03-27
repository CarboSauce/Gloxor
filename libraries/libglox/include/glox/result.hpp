#pragma once
#include "assert.hpp"
#include "detail/opnew.hpp"
#include "macros.hpp"
#include "metaprog.hpp"
#include <compare>
#include <type_traits>
namespace glox
{
struct empty_t
{
};

template <typename T, typename E, bool = false>
struct pod_union
{
	static constexpr bool is_pod = false;
	union
	{
		T _val;
	};
	E _err;
	constexpr pod_union() = default;
	constexpr explicit pod_union(T&& _val) : _val(RVALUE(_val)), _err(E{}) {}
	constexpr explicit pod_union(const T& _val) : _val(_val), _err(E{}) {}
	constexpr pod_union(const pod_union& other) : _err(other._err)
	{
		if (not other._err)
			new (&_val) T(other._val);
	}
	constexpr pod_union(pod_union&& other) : _err(RVALUE(other._err))
	{
		if (not other._err)
			new (&_val) T(RVALUE(other._val));
	}
	// pod_union& operator=(const pod_union& other) = delete;
	// pod_union& operator=(pod_union&& other) = delete;
	constexpr pod_union& operator=(const pod_union& other)
	{
		if (other._err)
			return *this;
		if (not _err)
			_val.~T();
		_val = other._val; // new (&a.val) T(other.a.val);
		_err = other._err;
		return *this;
	}
	constexpr pod_union& operator=(pod_union&& other)
	{
		if (other._err)
			return *this;
		if (not _err)
			_val.~T();
		// new (&_val) T(RVALUE(other.a.val));
		_val = RVALUE(other._val);
		_err = RVALUE(other._err);
		return *this;
	}
	constexpr ~pod_union()
	{
		if (not _err)
			_val.~T();
	}
};
template <typename T, typename E>
struct pod_union<T, E, true>
{
	static constexpr bool is_pod = true;
	GLOX_ALWAYS_INLINE constexpr explicit pod_union(E&& _err) : _err{RVALUE(_err)} {}
	GLOX_ALWAYS_INLINE constexpr explicit pod_union(T&& _val) : _val{RVALUE(_val)}, _err{E{}} {}
	GLOX_ALWAYS_INLINE constexpr explicit pod_union(const T& _val) : _val{_val}, _err{E{}} {}
	union
	{
		T _val;
	};
	E _err;
};
enum option_t : bool
{
	some = false,
	none = true
};
GLOX_ALWAYS_INLINE
inline constexpr option_t operator!(option_t a)
{
	return static_cast<option_t>(!static_cast<bool>(a));
}

template <typename T, typename E = option_t>
class [[nodiscard]] result : private pod_union<T, E, std::is_trivially_copyable_v<T>>
{
	// static_assert(std::is_default_constructible<T>::value, "Result type needs to be default constructible");
	// static_assert(std::is_trivially_copyable_v<T> and std::is_trivially_destructible_v<T>,"T needs to be POD");
	static_assert(std::is_trivial<E>::value, "Error is required to be trivial type");
	using storage_t = pod_union<T, E, std::is_trivially_copyable_v<T>>;
	using storage_t::_err;
	using storage_t::_val;
	constexpr static bool is_conv =
		 std::is_convertible<T, E>::value;
	//! std::is_constructible<T,E>::value;
 public:
	// static constexpr bool is_pod = storage_t::is_pod;
	// friend conditional_trivial<T, E,std::is_trivially_copyable_v<T>>;
	GLOX_ALWAYS_INLINE constexpr explicit(is_conv)
		 result(T&& val) : storage_t{RVALUE(val)} {}
	GLOX_ALWAYS_INLINE constexpr explicit(is_conv)
		 result(const T& val) : storage_t{val} {}
	GLOX_ALWAYS_INLINE constexpr explicit(is_conv)
		 result(E err) : storage_t{RVALUE(err)} {}
	constexpr ~result() = default;
	constexpr result(const result&) = default;
	constexpr result(result&&) = default;
	constexpr result& operator=(const result&) = default;
	constexpr result& operator=(result&&) = default;
	GLOX_ALWAYS_INLINE
	static constexpr result from_err(E&& err)
	{
		return result{RVALUE(err)};
	}
	GLOX_ALWAYS_INLINE
	static constexpr result from_val(T&& val)
	{
		return result{RVALUE(val)};
	}
	constexpr T unwrap() &&
	{
		gloxAssert(!static_cast<bool>(_err), "Can't unwrap an error");
		return RVALUE(_val);
	}
	constexpr T& val() const
	{
		gloxAssert(!static_cast<bool>(_err), "Can't unwrap an error");
		return _val;
	}
	constexpr T& val()
	{
		gloxAssert(!static_cast<bool>(_err), "Can't unwrap an error");
		return _val;
	}
	constexpr T err() const
	{
		return _err;
	}
	constexpr bool has_value() const
	{
		return !_err;
	}
	constexpr operator bool() const
	{
		return static_cast<bool>(!_err);
	}
	constexpr auto operator<=>(const result& b)
	{
		if (!_err and !b._err)
			return _val <=> b._val;
		return _err <=> b._err;
	}
	constexpr bool operator==(const result&) const = default;
	constexpr auto operator==(const E& b) const
	{
		return _err == b._err;
	}
	constexpr T unwrap_or(T&& def) &&
	{
		if (!_err)
			return RVALUE(_val);
		return RVALUE(def);
	}
	constexpr T& val_or(T&& def)
	{
		if (!_err)
			return _val;
		return def;
	}
	constexpr T& val_or(T&& def) const
	{
		if (!_err)
			return _val;
		return def;
	}
};
template <typename T>
using optional = result<T, option_t>;
// template<typename T>
// struct option : result<T,option_t>
//{
//	using result<T,option_t>::result;
//
// };
// template <typename T>
// using optional = result<T, option_t>;
// template <typename T>
// result(T) -> result<T,option_t>;
} // namespace glox
