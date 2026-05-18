#pragma once

#include "ZmeyaBuilderBase.h"

namespace zm
{

/*

**ArenaRef<T>**

Session-bound handle to an object inside the write-path arena (`detail::BuilderBase::data`). Stores a
**byte offset** plus the active builder pointer so the address stays valid across `std::vector`
reallocations of the arena.

Use **`operator->` / `operator*`** for normal field access (each use re-resolves through the current
buffer). Call **`transient_ptr()`** only when you need a raw **`T*`** for an external API, a
container of pointers, **`zm::Pointer` assignment**, or similar; do **not** store that raw pointer
across operations that can grow the arena.

*/

template <typename T> class ArenaRef
{
    detail::BuilderBase* builder_ = nullptr;
    goffset_t byte_offset_ = 0;

  public:
    ArenaRef() noexcept = default;

    ArenaRef(detail::BuilderBase* builder, goffset_t byte_offset) noexcept
        : builder_(builder)
        , byte_offset_(byte_offset)
    {
    }

    ZMEYA_NODISCARD T* transient_ptr() const noexcept
    {
        if (builder_ == nullptr)
        {
            return nullptr;
        }
        return reinterpret_cast<T*>(builder_->get_ptr_unsafe_to_store(byte_offset_));
    }

    ZMEYA_NODISCARD T* operator->() const noexcept { return transient_ptr(); }

    ZMEYA_NODISCARD T& operator*() const noexcept
    {
        T* p = transient_ptr();
        ZMEYA_HARD_ASSERT(p != nullptr);
        return *p;
    }

    ZMEYA_NODISCARD explicit operator bool() const noexcept { return builder_ != nullptr; }
};

namespace detail
{

template <typename T> inline ArenaRef<T> allocate_arena_ref(BuilderBase* b)
{
    static_assert(std::is_trivially_copyable<T>::value, "allocate_arena_ref: T must be trivially copyable");
    ZMEYA_ASSERT(b != nullptr);
    const goffset_t g_offs = b->alloc_aligned(sizeof(T), alignof(T));
    void* const ptr = b->get_ptr_unsafe_to_store(g_offs);
    BuilderBase::placementCtor<T>(ptr);
    return ArenaRef<T>(b, g_offs);
}

} // namespace detail

} // namespace zm
