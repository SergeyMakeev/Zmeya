#pragma once

#include "ZmeyaConfig.h"
#include <cstdint>

namespace zm
{

/*

**Forward declaration**

`BlobLayoutValidator` walks blob bytes for untrusted-input checks; containers grant it private access.

*/

struct BlobLayoutValidator;

using offset_t = std::uintptr_t;
using diff_t = std::ptrdiff_t;
using roffset_t = int32_t;

namespace detail
{

/*

**Self-relative address**

`slot + (ptrdiff_t)offset` can wrap `uintptr_t` on hostile values. Validators use the bool-returning form;
hot paths use `toAbsoluteAddr` which hard-asserts on overflow (integration bugs or pre-validation reads).

*/

ZMEYA_NODISCARD inline bool self_rel_target_address(uintptr_t slot, roffset_t offset, uintptr_t* out_target) noexcept
{
    const uintptr_t su = slot;
    if (offset >= 0)
    {
        const uintptr_t mag = static_cast<uintptr_t>(static_cast<int64_t>(offset));
        if (mag > (UINTPTR_MAX - su))
        {
            return false;
        }
        *out_target = su + mag;
        return true;
    }
    const int64_t neg = -static_cast<int64_t>(offset);
    const uintptr_t mag = static_cast<uintptr_t>(neg);
    if (mag > su)
    {
        return false;
    }
    *out_target = su - mag;
    return true;
}

} // namespace detail

ZMEYA_NODISCARD inline uintptr_t toAbsoluteAddr(uintptr_t base, roffset_t offset)
{
    uintptr_t out = base;
    const bool ok = detail::self_rel_target_address(base, offset, &out);
    ZMEYA_HARD_ASSERT(ok && "self-relative offset overflow");
    return out;
}

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
namespace detail
{
class BuilderBase;
}
#endif

template <typename Key> class HashSet;
template <typename Key, typename Value> class HashMap;

template <typename T> class Pointer;

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
template <typename T> void assign(Pointer<T>& _to, T* from);
#endif

class String;

template <typename T> class Array;

} // namespace zm
