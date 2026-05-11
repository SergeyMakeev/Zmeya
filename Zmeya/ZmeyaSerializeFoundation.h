#pragma once

#include "ZmeyaHashMap.h"
#include <limits>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace zm
{

ZMEYA_NODISCARD inline diff_t diff(offset_t a, offset_t b) noexcept
{
    diff_t res = a - b;
    return res;
}

ZMEYA_NODISCARD inline offset_t diffAddr(uintptr_t a, uintptr_t b)
{
    ZMEYA_ASSERT(a >= b);
    uintptr_t res = a - b;
    ZMEYA_ASSERT(res <= uintptr_t(std::numeric_limits<offset_t>::max()));
    return offset_t(res);
}

ZMEYA_NODISCARD inline roffset_t toRelativeOffset(diff_t v)
{
    ZMEYA_ASSERT(v >= diff_t(std::numeric_limits<roffset_t>::min()));
    ZMEYA_ASSERT(v <= diff_t(std::numeric_limits<roffset_t>::max()));
    return roffset_t(v);
}

constexpr bool inline isPowerOfTwo(size_t v) { return v && ((v & (v - 1)) == 0); }

/*

**Aligned backing allocator**

The blob arena is a `std::vector<char, BufferAllocator<..., ZMEYA_MAX_ALIGN>>` so reallocations stay
max-aligned without depending on implementation-defined `std::allocator` alignment behavior.

*/

template <typename T, int Alignment> class BufferAllocator : public std::allocator<T>
{
  public:
    typedef size_t size_type;
    typedef T* pointer;
    typedef const T* const_pointer;

    template <typename _Tp1> struct rebind
    {
        typedef BufferAllocator<_Tp1, Alignment> other;
    };

    pointer allocate(size_type n)
    {
        const size_t alignment = Alignment;
        void* const pv = ZMEYA_ALLOC(n * sizeof(T), alignment);
        ZMEYA_HARD_ASSERT(pv != nullptr && "ZMEYA_ALLOC returned null; supply an allocator that throws or aborts on failure");
        return static_cast<pointer>(pv);
    }

    void deallocate(pointer p, size_type)
    {
        ZMEYA_FREE(p);
    }

    BufferAllocator()
        : std::allocator<T>()
    {
    }
    BufferAllocator(const BufferAllocator& a)
        : std::allocator<T>(a)
    {
    }
    template <class U>
    BufferAllocator(const BufferAllocator<U, Alignment>& a)
        : std::allocator<T>(a)
    {
    }
};

/*

**Owning blob byte buffer**

Same allocator as the write-path arena (`BufferAllocator`). `write_blob` returns this type so the
finalized bytes can be moved out without copying the arena into a separate `std::vector<char>`.

*/

using BlobBuffer = std::vector<char, BufferAllocator<char, ZMEYA_MAX_ALIGN>>;

/*

**Span view**

Returned from `finalize` so callers can copy bytes without exposing the internal vector type.

*/

template <typename T> struct Span
{
    T* data = nullptr;
    size_t size = 0;

    Span() = default;
    Span(T* _data, size_t _size)
        : data(_data)
        , size(_size)
    {
    }
};

/*

**Global offsets inside the arena**

`goffset_t` aliases `roffset_t` but reads as "offset into the serialized blob buffer" at patch time.

*/

using goffset_t = roffset_t;

template <typename T> constexpr T highest_bit()
{
    static_assert(std::is_integral_v<T>, "T must be an integral type");
    using U = std::make_unsigned_t<T>;
    return T(U(1) << (std::numeric_limits<U>::digits - 1));
}

namespace detail
{

class BuilderBase;

/*

**Default write arena capacity**

`std::vector<char>` for the blob writer starts with a modest reserve so typical small blobs avoid an
immediate growth step. This is not a correctness knob: user code must not cache raw pointers into the
arena across operations that can grow the buffer.

*/

inline constexpr size_t kDefaultWriteBlobArenaReserveBytes = size_t(64) * 1024;

inline thread_local BuilderBase* g_tls_active_builder = nullptr;

#if defined(_DEBUG) || defined(ZMEYA_DEBUG_TLS_BUILDER_STACK)
inline thread_local int g_tls_builder_stack_depth = 0;
#endif

inline BuilderBase* get_global_builder() noexcept { return g_tls_active_builder; }

inline void set_global_builder(BuilderBase* builder) noexcept { g_tls_active_builder = builder; }

/*

**Stack pointer guard**

`get_relative_offset` refuses stack addresses because their absolute location is not stable relative to
the arena. On non-Windows platforms this check is conservatively disabled.

*/

inline bool is_stack_pointer(const void* ptr)
{
#if defined(_WIN32)
    PVOID stack_low = nullptr;
    PVOID stack_high = nullptr;
    GetCurrentThreadStackLimits(reinterpret_cast<PULONG_PTR>(&stack_low), reinterpret_cast<PULONG_PTR>(&stack_high));

    auto p = reinterpret_cast<uintptr_t>(ptr);
    return p >= reinterpret_cast<uintptr_t>(stack_low) && p < reinterpret_cast<uintptr_t>(stack_high);
#else
    (void)ptr;
    return false;
#endif
}

class ScopedBuilder
{
  public:
    explicit ScopedBuilder(BuilderBase* builder)
    {
        ZMEYA_ASSERT(builder != nullptr);
#if defined(_DEBUG) || defined(ZMEYA_DEBUG_TLS_BUILDER_STACK)
        ++g_tls_builder_stack_depth;
#endif
        prev = get_global_builder();
        set_global_builder(builder);
    }

    ~ScopedBuilder()
    {
        set_global_builder(prev);
#if defined(_DEBUG) || defined(ZMEYA_DEBUG_TLS_BUILDER_STACK)
        --g_tls_builder_stack_depth;
#endif
    }

    ScopedBuilder(const ScopedBuilder&) = delete;
    ScopedBuilder& operator=(const ScopedBuilder&) = delete;

  private:
    BuilderBase* prev;
};

} // namespace detail

} // namespace zm
