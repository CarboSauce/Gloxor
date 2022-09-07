#pragma once

// Freestanding C++ environments usually dont have c<*lib*> versions
// This is not really portable but it works on major compilers
// Better solution needs to be done
#include <stddef.h>
#include <stdint.h>

namespace gx
{
using uintptr = uintptr_t;
using paddrT = uintptr_t;
using vaddrT = uintptr_t;
using sizeT = size_t;
using u8 = uint8_t;
using i8 = int8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
} // namespace gx

using gx::i16;
using gx::i32;
using gx::i64;
using gx::i8;
using gx::paddrT;
using gx::sizeT;
using gx::u16;
using gx::u32;
using gx::u64;
using gx::u8;
using gx::uintptr;
using gx::vaddrT;
