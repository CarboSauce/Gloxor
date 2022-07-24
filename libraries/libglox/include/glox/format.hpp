#pragma once
#include "glox/array.hpp"
#include "glox/types.hpp"
#include "glox/utilalgs.hpp"

namespace glox
{
	using callback_t = void (*)(const char*, size_t);
	namespace unsafe
	{

		inline auto format(char* buffer, const void* addr)
		{
			auto value = (uintptr_t)addr;
			size_t i = 0;
			buffer[i++] = '0';
			buffer[i++] = 'x';
			do
			{
				auto digit = value % 16;
				value /= 16;
				char hexDigit = "0123456789ABCDEF"[digit];
				buffer[i++] = hexDigit;
			} while (value > 0);
			glox::reverse(buffer + 2, buffer + i);
			return i;
		}

		inline auto format(char* buffer, uintmax_t value)
		{
			size_t i = 0;
			do
			{
				auto digit = value % 10;
				value /= 10;
				buffer[i++] = '0' + digit;
			} while (value > 0);
			glox::reverse(buffer, buffer + i);
			return i;
		}

		inline auto format(char* buffer, intmax_t value)
		{
			int signOffset = 0;
			uintmax_t uvalue = value;
			if (value < 0)
			{
				buffer[signOffset++] = '-';
				uvalue = -(uintmax_t)value;
			}
			return format(buffer + signOffset, uvalue) + signOffset;
		}

		inline auto format(char* buffer, char value)
		{
			*buffer = value;
			return (size_t)1;
		}

	} // namespace unsafe
} // namespace glox