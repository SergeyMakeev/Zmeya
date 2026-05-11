#pragma once

#include "ZmeyaHash.h"
#include "ZmeyaString.h"

namespace std
{
template <> struct hash<zm::String>
{
    size_t operator()(zm::String const& s) const noexcept
    {
        const char* str = s.c_str();
        return zm::HashUtils::hashString(str);
    }
};
} // namespace std
