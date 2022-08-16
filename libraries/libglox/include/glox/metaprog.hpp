#pragma once
#include <type_traits>
#define RVALUE(...) static_cast<std::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)
#define FORWARD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

#if __has_builtin(__builtin_bit_cast)
	#define BITCAST(T, from) __builtin_bit_cast(T, from)
#else
	#error Missing support for __builtin_bit_cast
#endif
