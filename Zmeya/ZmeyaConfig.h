#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*

**Build-time hooks**

`ZMEYA_EXTERNAL_HASH`, `ZMEYA_ALLOC` / `ZMEYA_FREE`, `ZMEYA_ENABLE_SERIALIZE_SUPPORT`,
`ZMEYA_NODISCARD`, `ZMEYA_FALLTHROUGH`, and `ZMEYA_ASSERT` stay overridable from the TU.

*/

#if !defined(ZMEYA_ALLOC) || !defined(ZMEYA_FREE)
#if defined(_WIN32)
#include <xmmintrin.h>
#define ZMEYA_ALLOC(sizeInBytes, alignment) _mm_malloc(sizeInBytes, alignment)
#define ZMEYA_FREE(ptr) _mm_free(ptr)
#elif defined(__ANDROID__)
#include <stdlib.h>
#define ZMEYA_ALLOC(sizeInBytes, alignment) memalign(alignment, sizeInBytes);
#define ZMEYA_FREE(ptr) free(ptr)
#else
#include <stdlib.h>
inline void* alloc_aligned_posix(size_t sizeInBytes, size_t alignment)
{
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, sizeInBytes) != 0)
    {
        return nullptr;
    }
    return ptr;
}
#define ZMEYA_ALLOC(sizeInBytes, alignment) alloc_aligned_posix(sizeInBytes, alignment)
#define ZMEYA_FREE(ptr) free(ptr)
#endif
#endif

#ifndef ZMEYA_ASSERT
namespace zm
{
void onAssertionFailed(const char* expression, const char* srcFile, unsigned int srcLine);
}

#define ZMEYA_ASSERT(expression) (void)((!!(expression)) || (zm::onAssertionFailed(#expression, __FILE__, (unsigned int)(__LINE__)), 0))
#endif

#ifndef ZMEYA_NODISCARD
#if __cplusplus >= 201703L
#define ZMEYA_NODISCARD [[nodiscard]]
#else
#define ZMEYA_NODISCARD
#endif
#endif

#ifndef ZMEYA_FALLTHROUGH
#if __cplusplus >= 201703L
#define ZMEYA_FALLTHROUGH [[fallthrough]]
#else
#define ZMEYA_FALLTHROUGH
#endif
#endif

#define ZMEYA_MAX_ALIGN (64)

#ifdef _DEBUG
#define ZMEYA_VALIDATE_HASH_DUPLICATES
#endif
