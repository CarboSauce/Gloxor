#pragma once

// Freestanding C++ environments usually dont have c<*lib*> versions
// This is not really portable but it works on major compilers
// Better solution needs to be done
#include <stddef.h>
#include <stdint.h>

namespace glox
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
} // namespace glox

using glox::paddrT;
using glox::vaddrT;
using glox::sizeT;
using glox::uintptr;
using glox::i16;
using glox::i32;
using glox::i64;
using glox::i8;
using glox::u16;
using glox::u32;
using glox::u64;
using glox::u8;
