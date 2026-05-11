#pragma once

#include "ZmeyaConfig.h"

namespace zm
{

#define ZMEYA_MURMURHASH_MAGIC64A 0xc6a4a7935bd1e995LLU

/*

**Murmur-style 64-bit hash**

Used for stable string hashing when the STL `std::hash` fingerprint is not desired.
The magic constant name is undefined after the function so it cannot leak into TUs.

*/

inline uint64_t murmur_hash_process64a(const char* key, uint32_t len, uint64_t seed)
{
    const uint64_t m = ZMEYA_MURMURHASH_MAGIC64A;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (len / 8);

    while (data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char* data2 = (const unsigned char*)data;

    switch (len & 7)
    {
    case 7:
        h ^= (uint64_t)((uint64_t)data2[6] << (uint64_t)48);
        ZMEYA_FALLTHROUGH;
    case 6:
        h ^= (uint64_t)((uint64_t)data2[5] << (uint64_t)40);
        ZMEYA_FALLTHROUGH;
    case 5:
        h ^= (uint64_t)((uint64_t)data2[4] << (uint64_t)32);
        ZMEYA_FALLTHROUGH;
    case 4:
        h ^= (uint64_t)((uint64_t)data2[3] << (uint64_t)24);
        ZMEYA_FALLTHROUGH;
    case 3:
        h ^= (uint64_t)((uint64_t)data2[2] << (uint64_t)16);
        ZMEYA_FALLTHROUGH;
    case 2:
        h ^= (uint64_t)((uint64_t)data2[1] << (uint64_t)8);
        ZMEYA_FALLTHROUGH;
    case 1:
        h ^= (uint64_t)((uint64_t)data2[0]);
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

#undef ZMEYA_MURMURHASH_MAGIC64A

#ifndef ZMEYA_EXTERNAL_HASH

/*

**Default hash helpers**

Replace this block with `ZMEYA_EXTERNAL_HASH` if your engine already exposes stable hashes.

*/

namespace HashUtils
{

template <typename T> ZMEYA_NODISCARD inline size_t hasher(const T& v) { return std::hash<T>{}(v); }

ZMEYA_NODISCARD inline size_t hashString(const char* str)
{
    size_t len = std::strlen(str);
    uint64_t hash = zm::murmur_hash_process64a(str, uint32_t(len), 13061979);
    return size_t(hash);
}
} // namespace HashUtils
#endif

} // namespace zm
