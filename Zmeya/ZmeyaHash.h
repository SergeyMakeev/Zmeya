#pragma once

#include "ZmeyaConfig.h"
#include <limits>

namespace zm
{

#define ZMEYA_MURMURHASH_MAGIC64A 0xc6a4a7935bd1e995LLU

/*

**Murmur-style 64-bit hash**

Uses `memcpy` for 8-byte chunks so misaligned blob string starts do not fault on strict-align platforms
and to avoid strict-aliasing issues from casting to `uint64_t*`.

The magic constant name is undefined after the function so it cannot leak into TUs.

*/

inline uint64_t murmur_hash_process64a(const char* key, uint32_t len, uint64_t seed)
{
    const uint64_t m = ZMEYA_MURMURHASH_MAGIC64A;
    const int r = 47;

    uint64_t h = seed ^ (uint64_t(len) * m);

    size_t off = 0;
    while (off + 8 <= size_t(len))
    {
        uint64_t k = 0;
        std::memcpy(&k, key + off, sizeof(uint64_t));
        off += 8;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char* tail = reinterpret_cast<const unsigned char*>(key + off);

    switch (len & 7)
    {
    case 7:
        h ^= (uint64_t)((uint64_t)tail[6] << (uint64_t)48);
        ZMEYA_FALLTHROUGH;
    case 6:
        h ^= (uint64_t)((uint64_t)tail[5] << (uint64_t)40);
        ZMEYA_FALLTHROUGH;
    case 5:
        h ^= (uint64_t)((uint64_t)tail[4] << (uint64_t)32);
        ZMEYA_FALLTHROUGH;
    case 4:
        h ^= (uint64_t)((uint64_t)tail[3] << (uint64_t)24);
        ZMEYA_FALLTHROUGH;
    case 3:
        h ^= (uint64_t)((uint64_t)tail[2] << (uint64_t)16);
        ZMEYA_FALLTHROUGH;
    case 2:
        h ^= (uint64_t)((uint64_t)tail[1] << (uint64_t)8);
        ZMEYA_FALLTHROUGH;
    case 1:
        h ^= (uint64_t)((uint64_t)tail[0]);
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

#undef ZMEYA_MURMURHASH_MAGIC64A

#ifndef ZMEYA_HASH_ADAPTER_CSTR_MAX_SCAN
#define ZMEYA_HASH_ADAPTER_CSTR_MAX_SCAN (size_t(1) << 20)
#endif

ZMEYA_NODISCARD inline size_t bounded_cstring_byte_length(const char* p, size_t max_scan) noexcept
{
    if (p == nullptr)
    {
        return 0;
    }
    for (size_t i = 0; i < max_scan; ++i)
    {
        if (p[i] == '\0')
        {
            return i;
        }
    }
    return max_scan;
}

#ifndef ZMEYA_EXTERNAL_HASH

/*

**Default hash helpers**

Replace this block with `ZMEYA_EXTERNAL_HASH` if your engine already exposes stable hashes.

`hashString` assumes a trusted NUL-terminated C string. For probes with an unknown upper bound,
use `hashCStringBounded` or `hashStringBytes`.

*/

namespace HashUtils
{

template <typename T> ZMEYA_NODISCARD inline size_t hasher(const T& v) { return std::hash<T>{}(v); }

ZMEYA_NODISCARD inline size_t hashStringBytes(const char* data, size_t len) noexcept
{
    const uint32_t len32 = (len > size_t(std::numeric_limits<uint32_t>::max())) ? std::numeric_limits<uint32_t>::max() : uint32_t(len);
    uint64_t hash = zm::murmur_hash_process64a(data, len32, 13061979);
    return size_t(hash);
}

ZMEYA_NODISCARD inline size_t hashCStringBounded(const char* str, size_t max_scan) noexcept
{
    const size_t len = zm::bounded_cstring_byte_length(str, max_scan);
    return hashStringBytes(str, len);
}

ZMEYA_NODISCARD inline size_t hashString(const char* str)
{
    const size_t len = std::strlen(str);
    return hashStringBytes(str, len);
}
} // namespace HashUtils
#endif

} // namespace zm
