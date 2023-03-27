#include <stddef.h>
#include <stdint.h>

extern "C" {
void* memset(void* dst, int val, size_t size)
{
	auto* mems = (uint8_t*)dst;
	for (size_t i = 0; i < size; ++i) {
		mems[i] = val;
	}
	return dst;
}

void* memcpy(void* __restrict__ dst, const void* __restrict__ src, size_t size)
{
	auto* d = (uint8_t*)dst;
	const auto* s = (const uint8_t*)src;
	for (size_t i = 0; i < size; ++i) {
		d[i] = s[i];
	}
	return d;
}

size_t strlen(const char* str)
{
	size_t acm = 0;
	for (; str[acm] != 0; ++acm)
		;
	return acm;
}

void* memmove(void* dest, const void* src, size_t len)
{
	char* d = (char*)dest;
	const char* s = (char*)src;
	if (d < s)
		while (len--)
			*d++ = *s++;
	else {
		const char* lasts = s + (len - 1);
		char* lastd = d + (len - 1);
		while (len--)
			*lastd-- = *lasts--;
	}
	return dest;
}
}