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

  private:
    bool isEqual(const Pointer& other) const noexcept { return get() == other.get(); }

    ZMEYA_NODISCARD T* getUnsafe() const noexcept
    {
        uintptr_t self = uintptr_t(this);
        ZMEYA_ASSERT(relativeOffset != 0);
        uintptr_t addr = toAbsoluteAddr(self, relativeOffset);
        return reinterpret_cast<T*>(addr);
    }

  public:
    Pointer() noexcept = default;

    ZMEYA_NODISCARD T* get() const noexcept
    {
        uintptr_t self = uintptr_t(this);
        if (relativeOffset == 0)
        {
            return nullptr;
        }
        uintptr_t addr = toAbsoluteAddr(self, relativeOffset);
        return reinterpret_cast<T*>(addr);
    }

    ZMEYA_NODISCARD T* operator->() const noexcept { return getUnsafe(); }

    ZMEYA_NODISCARD T& operator*() const noexcept { return *(getUnsafe()); }

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
