#include <cstdint>
#include <cstddef>

#undef memcpy
#undef memset

extern "C"
{

void* memset(void* dst, int val, size_t size) 
{
    auto* mems = (uint8_t*)dst;
    for(size_t i = 0; i < size; ++i)
    {
        mems[i] = val;
    }
    return dst;
}


void* memcpy(void* dst, const void* src, size_t size)
{
    auto* d = (uint8_t*)dst;
    const auto* s = (const uint8_t*)src;
    for (size_t i = 0; i < size; ++i)
    {
        d[i] = s[i];
    }
    return d;
}

}
