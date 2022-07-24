#pragma once
#include "glox/types.hpp"

namespace glox
{
	/**
	 * @brief Implementation of djb2 hash 
	 * @param str String for which hash is to be computed, UB if string is null
	 * @link http://www.cse.yorku.ca/~oz/hash.html 
 	 */
	constexpr uint32_t djb2_hash(const char* str)
	{
		uint32_t hash = 5381; //magic prime number
		uint8_t c;
		while ((c = *str++))
			hash = ((hash << 5) + hash) ^ c;
		return hash;
	}


	/**
	 * @brief Implementation of FNV-1a hash
 	 * @link https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
	 * @param str String for which hash is to be computed, UB if string is null
	 */
	constexpr uint32_t fnv_hash(const char* str)
	{
		uint32_t hash = 0x811c9dc5; //magic prime
		uint8_t c;
		while ((c = *str++))
		{
			hash = (hash * 0x01000193) ^ c; //magic Prime 2^24 + 2^8 + 0x93
		}
		return hash;
	}

	constexpr auto fxdPointFactor = 8;

	/**
	 * @brief Linear interpolation using fixed point arithmetic from [a,b] to [0,bound]
	 * @tparam bound Bound of lerp
	 * @param a Lower bound of lerp
	 * @param b Upper bound of lerp
	 * @param time 
	 */
	template <typename T, T bound>
	constexpr T fixed_lerp(T a, T b, T time)
	{
		return a + ((time * fxdPointFactor) * (b - a) / bound) / fxdPointFactor;
	}

} // namespace glox