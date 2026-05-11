#pragma once

#include "ZmeyaPointer.h"

namespace zm
{

void assign(String& to, const std::string& from);
void assign(String& to, const char* from);

/*

**Blob string**

Payload is referenced through `Pointer<char>` so the character data can live elsewhere in the blob.

**Invalidation (same idea as `std::string`)**

During `write_blob` / `BlobWriter`, do not keep a `const char*` from `c_str()` across `append` /
`operator+=` / `clear` on the same `String`. Use `w.root()->...` each time. After finalize, a `const`
view over the returned blob is stable for that buffer.

*/

class String
{
  public:
    Pointer<char> data;
    String() noexcept = default;

    friend struct BlobLayoutValidator;

    bool isEqual(const char* s2) const noexcept
    {
        const char* s1 = c_str();
        return (std::strcmp(s1, s2) == 0);
    }

    ZMEYA_NODISCARD const char* c_str() const noexcept
    {
        const char* v = data.get();
        if (v != nullptr)
        {
            return v;
        }
        return "";
    }

    ZMEYA_NODISCARD bool empty() const noexcept { return data.get() == nullptr; }
    ZMEYA_NODISCARD bool operator==(const String& other) const noexcept
    {
        if (other.c_str() == c_str())
        {
            return true;
        }
        return isEqual(other.c_str());
    }
    ZMEYA_NODISCARD bool operator!=(const String& other) const noexcept
    {
        if (other.c_str() == c_str())
        {
            return false;
        }
        return !isEqual(other.c_str());
    }

    String& operator=(const std::string& other)
    {
        assign(*this, other);
        return *this;
    }

    String& operator=(const char* other)
    {
        assign(*this, other);
        return *this;
    }

    friend void assign(String& to, const std::string& from);
    friend void assign(String& to, const char* from);

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    void clear();
    String& append(const char* suf);
    String& append(const char* suf, size_t suf_len);
    ZMEYA_NODISCARD String& operator+=(const char* suf);
#endif
};

ZMEYA_NODISCARD inline bool operator==(const String& left, const char* const right) noexcept { return left.isEqual(right); }
ZMEYA_NODISCARD inline bool operator!=(const String& left, const char* const right) noexcept { return !left.isEqual(right); }

ZMEYA_NODISCARD inline bool operator==(const char* const left, const String& right) noexcept { return right.isEqual(left); }
ZMEYA_NODISCARD inline bool operator!=(const char* const left, const String& right) noexcept { return !right.isEqual(left); }

ZMEYA_NODISCARD inline bool operator==(const String& left, const std::string& right) noexcept { return left.isEqual(right.c_str()); }
ZMEYA_NODISCARD inline bool operator!=(const String& left, const std::string& right) noexcept { return !left.isEqual(right.c_str()); }

ZMEYA_NODISCARD inline bool operator==(const std::string& left, const String& right) noexcept { return right.isEqual(left.c_str()); }
ZMEYA_NODISCARD inline bool operator!=(const std::string& left, const String& right) noexcept { return !right.isEqual(left.c_str()); }

} // namespace zm
