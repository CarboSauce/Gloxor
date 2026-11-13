#ifndef LIBGLOX_OPERATOR_NEW
#define LIBGLOX_OPERATOR_NEW
// include small subset of whats needed in libglox
#ifndef USE_MODULES
#include <new>
#if defined(_LIBCPP_VERSION)
#include <__memory/allocator.h>
#include <__memory/construct_at.h>
#else
#include <bits/allocator.h>
#include <bits/stl_construct.h>
#endif
#endif

namespace glox {
GLOX_BEGIN_EXPORT
struct in_place_t
{
    explicit in_place_t() = default;
};
inline constexpr auto in_place = in_place_t { };
template <typename T>
struct in_place_type_t
{
    explicit in_place_type_t() = default;
};
template <typename T>
constexpr in_place_type_t<T> in_place_type { };
GLOX_END_EXPORT
} // namespace glox

#endif
