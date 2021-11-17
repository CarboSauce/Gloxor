#include <glox/algo.hpp>

namespace glox
{

	uint32_t djb2_hash(const char* str)
	{
		uint32_t _hash = 5381; /*magic prime number*/
		uint8_t _cached;
		while ((_cached = *str++))
			_hash = ((_hash << 5) + _hash) ^ _cached;
		return _hash;
	}

	uint32_t fnv_hash(const char* str)
	{
		uint32_t _hash = 0x811c9dc5; //magic prime
		uint8_t _c;
		while ((_c = *str++))
		{
			_hash = (_hash * 0x01000193) ^ _c; //magic Prime 2^24 + 2^8 + 0x93
		}
		return _hash;
	}

} // namespace glox