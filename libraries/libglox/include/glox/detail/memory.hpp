#ifndef LIBGLOX_OPERATOR_NEW
#define LIBGLOX_OPERATOR_NEW
// include small subset of whats needed in libglox
#include <new>
#if defined(_LIBCPP_VERSION)
#include <__memory/allocator.h>
#include <__memory/construct_at.h>
#else
#include <bits/allocator.h>
#include <bits/stl_construct.h>
#endif

namespace glox {
struct in_place_t
{ };
inline constexpr auto in_place = in_place_t { };
}

#endif
