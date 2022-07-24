#pragma once
#include "glox/format.hpp"
#include "glox/string.hpp"
#include "glox/types.hpp"

namespace glox
{
// constexpr auto whatever = ([]() constexpr
// 						   {
// 							   char buf[30];
// 							   return glox::unsafe::format(buf, (intmax_t)123);
// 						   })();
using namespace glox::unsafe;

/**
 * @brief Base Class used for ADL, derive from that class and implement
 * write function
 */
struct bStream
{
};

template <typename BStream, typename T>
inline BStream& operator,(BStream& out, const T& val)
{
	write(out, out.buffer, format(out.buffer, val));
	return out;
}

#define defineComma(x)                                 \
	template <typename BStream>                         \
	constexpr BStream& operator,(BStream& out, x val)   \
	{                                                   \
		write(out, out.buffer, format(out.buffer, val)); \
		return out;                                      \
	}

#define defineCommaCast(x, to)                             \
	template <typename BStream>                             \
	constexpr BStream& operator,(BStream& out, x val)       \
	{                                                       \
		write(out, out.buffer, format(out.buffer, (to)val)); \
		return out;                                          \
	}
// clang-format off
		defineComma(char)
		defineComma(uintmax_t)
		defineComma(intmax_t)
		defineCommaCast(uint32_t,uintmax_t)
		defineCommaCast(uint16_t,uintmax_t)
		defineCommaCast(uint8_t,uintmax_t)      
		defineCommaCast(int16_t,intmax_t)
		defineCommaCast(int32_t,intmax_t)
		defineCommaCast(bool,uintmax_t)

		template<typename BStream> 
		inline BStream& operator,(BStream& out,const void* val)
		{
			write(out, out.buffer, glox::format(out.buffer,val));
			return out;
		}


		// Cost of IO will most likely outweight cost of double looping
		// because of strlen, but potential compile time strlen 
		// should be more worth it
		template<typename BStream>
		constexpr BStream& operator,(BStream& out, const char* str)
		{
			write(out,str,glox::strlen(str));
			return out;
		}
// clang-format on

#undef defineComma
#undef defineCommaCast

} // namespace glox
