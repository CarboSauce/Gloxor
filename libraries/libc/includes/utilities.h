#ifndef CARBOSLIB_UTILITIES_H_
#define CARBOSLIB_UTILITIES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Implementation of djb2 hash 
 *  @link http://www.cse.yorku.ca/~oz/hash.html
 *  @param str String for which hash is to be computed, UB if string is null
 */
uint32_t djb2_hash(const char *str)
{
    uint32_t _hash = 5381; /*magic prime number*/ 
    uint8_t _cached;
    while ( (_cached = *str++)) 
        _hash = ((_hash << 5) + _hash) ^ _cached;
    return _hash;
}

/**
 * Implementation of FNV-1a hash
 * @link https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
 * @param str String for which hash is to be computed, UB if string is null
 */
uint32_t fnv_hash(const char* str)
{
    uint32_t _hash = 0x811c9dc5; //magic prime
    uint8_t _c; 
    while( ( _c = *str++) )
    {
        _hash = ( _hash * 0x01000193 ) ^ _c; //magic Prime 2^24 + 2^8 + 0x93 
    }
    return _hash;
}

#ifdef __cplusplus
}
#endif 


#endif 
