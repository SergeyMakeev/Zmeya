#pragma once

#include "ZmeyaConfig.h"

namespace zm
{

using offset_t = std::uintptr_t;
using diff_t = std::ptrdiff_t;
using roffset_t = int32_t;

ZMEYA_NODISCARD inline offset_t toAbsolute(offset_t base, roffset_t offset)
{
    offset_t res = base + diff_t(offset);
    return res;
}

ZMEYA_NODISCARD inline uintptr_t toAbsoluteAddr(uintptr_t base, roffset_t offset)
{
    uintptr_t res = base + ptrdiff_t(offset);
    return res;
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
