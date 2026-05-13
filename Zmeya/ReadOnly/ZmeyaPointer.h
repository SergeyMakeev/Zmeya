#pragma once

#include "ZmeyaTypes.h"

namespace zm
{

/*

**Self-relative pointer**

The slot stores an offset from its own address to the target. A null pointer is encoded as offset 0,
which means the type cannot point at itself (that would also be a zero offset).

*/

template <typename T> class Pointer
{
  public:
    roffset_t relativeOffset;

    friend struct BlobLayoutValidator;

  private:
    bool isEqual(const Pointer& other) const noexcept { return get() == other.get(); }

  public:
    Pointer() noexcept = default;

    ZMEYA_NODISCARD T* get() const noexcept
    {
        uintptr_t self = uintptr_t(this);
        if (relativeOffset == 0)
        {
            return nullptr;
        }
        uintptr_t addr = 0;
        if (!detail::self_rel_target_address(self, relativeOffset, &addr))
        {
            ZMEYA_HARD_ASSERT(false && "self-relative pointer offset overflow");
            return nullptr;
        }
        return reinterpret_cast<T*>(addr);
    }

    ZMEYA_NODISCARD T* try_get_in_blob(const std::byte* blob_begin, size_t blob_size) const noexcept
    {
        if (relativeOffset == 0)
        {
            return nullptr;
        }
        const uintptr_t b = reinterpret_cast<uintptr_t>(blob_begin);
        const uintptr_t e = b + blob_size;
        const uintptr_t self = reinterpret_cast<uintptr_t>(this);
        if (self < b || self + sizeof(Pointer<T>) > e)
        {
            return nullptr;
        }
        uintptr_t addr = 0;
        if (!detail::self_rel_target_address(self, relativeOffset, &addr))
        {
            return nullptr;
        }
        if (addr < b || addr + sizeof(T) > e)
        {
            return nullptr;
        }
        if ((addr % alignof(T)) != 0)
        {
            return nullptr;
        }
        return reinterpret_cast<T*>(addr);
    }

    ZMEYA_NODISCARD T* operator->() const noexcept
    {
        ZMEYA_ASSERT(relativeOffset != 0);
        return get();
    }

    ZMEYA_NODISCARD T& operator*() const noexcept
    {
        ZMEYA_ASSERT(relativeOffset != 0);
        return *get();
    }

    ZMEYA_NODISCARD bool operator==(const Pointer& other) const noexcept { return isEqual(other); }
    ZMEYA_NODISCARD bool operator!=(const Pointer& other) const noexcept { return !isEqual(other); }

    operator bool() const noexcept { return relativeOffset != 0; }
    ZMEYA_NODISCARD bool operator==(std::nullptr_t) const noexcept { return relativeOffset == 0; }
    ZMEYA_NODISCARD bool operator!=(std::nullptr_t) const noexcept { return relativeOffset != 0; }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    Pointer& operator=(T* from)
    {
        assign(*this, from);
        return *this;
    }
#endif
};

} // namespace zm
