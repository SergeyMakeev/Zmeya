// The MIT License (MIT)
//
// Copyright (c) 2021-2025 Sergey Makeev
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#pragma once

#include <algorithm>
#include <array>
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

#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// If this is not defined, the built-in hash will be used instead of the user-provided hash
// ZMEYA_EXTERNAL_HASH
//
//
// Override built-in memory allocation functionbs
// ZMEYA_ALLOC
// ZMEYA_FREE
//
//
// This macro is only necessary if you want to create serializable data.
// If you only need to read Zmeya blobs, you don't need it.
// ZMEYA_ENABLE_SERIALIZE_SUPPORT
//
//
// To override NODISCARD
// ZMEYA_NODISCARD
//
//
// To override FALLTHROUGH
// ZMEYA_FALLTHROUGH
//
//
// To override ASSERT
// ZMEYA_ASSERT
//
//

#if !defined(ZMEYA_ALLOC) || !defined(ZMEYA_FREE)
#if defined(_WIN32)
// Windows
#include <xmmintrin.h>
#define ZMEYA_ALLOC(sizeInBytes, alignment) _mm_malloc(sizeInBytes, alignment)
#define ZMEYA_FREE(ptr) _mm_free(ptr)
#elif defined(__ANDROID__)
// Android
#include <stdlib.h>
#define ZMEYA_ALLOC(sizeInBytes, alignment) memalign(alignment, sizeInBytes);
#define ZMEYA_FREE(ptr) free(ptr)
#else
// Posix
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
// #define ZMEYA_ASSERT(expression) assert(expression)

namespace zm
{
void onAssertionFailed(const char* expression, const char* srcFile, unsigned int srcLine);
} // namespace zm

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

namespace zm
{

#define ZMEYA_MURMURHASH_MAGIC64A 0xc6a4a7935bd1e995LLU

inline uint64_t murmur_hash_process64a(const char* key, uint32_t len, uint64_t seed)
{
    const uint64_t m = ZMEYA_MURMURHASH_MAGIC64A;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (len / 8);

    while (data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char* data2 = (const unsigned char*)data;

    switch (len & 7)
    {
    case 7:
        h ^= (uint64_t)((uint64_t)data2[6] << (uint64_t)48);
        ZMEYA_FALLTHROUGH;
    case 6:
        h ^= (uint64_t)((uint64_t)data2[5] << (uint64_t)40);
        ZMEYA_FALLTHROUGH;
    case 5:
        h ^= (uint64_t)((uint64_t)data2[4] << (uint64_t)32);
        ZMEYA_FALLTHROUGH;
    case 4:
        h ^= (uint64_t)((uint64_t)data2[3] << (uint64_t)24);
        ZMEYA_FALLTHROUGH;
    case 3:
        h ^= (uint64_t)((uint64_t)data2[2] << (uint64_t)16);
        ZMEYA_FALLTHROUGH;
    case 2:
        h ^= (uint64_t)((uint64_t)data2[1] << (uint64_t)8);
        ZMEYA_FALLTHROUGH;
    case 1:
        h ^= (uint64_t)((uint64_t)data2[0]);
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

#undef ZMEYA_MURMURHASH_MAGIC64A

#ifndef ZMEYA_EXTERNAL_HASH
//
// Functions and types to interact with the application
// Feel free to replace them to your engine specific functions
//
namespace HashUtils
{

// hasher
template <typename T> ZMEYA_NODISCARD inline size_t hasher(const T& v) { return std::hash<T>{}(v); }

// string hasher
ZMEYA_NODISCARD inline size_t hashString(const char* str)
{
    size_t len = std::strlen(str);
    uint64_t hash = zm::murmur_hash_process64a(str, uint32_t(len), 13061979);
    return size_t(hash);
}
} // namespace HashUtils
#endif

// absolute offset/difference type
using offset_t = std::uintptr_t;
using diff_t = std::ptrdiff_t;

// relative offset type
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

// Forward declarations for friend functions
template <typename Key> class HashSet;
template <typename Key, typename Value> class HashMap;

template <typename T> class Pointer;

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
template <typename T> void assign(Pointer<T>& _to, T* from);
#endif

/*
    Pointer - self-relative pointer relative to its own memory address
*/
template <typename T> class Pointer
{
  public:
    // addr = this + offset
    // offset(0) = this = nullptr (here is the limitation, pointer can't point to itself)
    // this extra offset fits well into the x86/ARM addressing modes
    // see for details  https://godbolt.org/z/aTTW9E7o9
    roffset_t relativeOffset;

  private:
    bool isEqual(const Pointer& other) const noexcept { return get() == other.get(); }

    ZMEYA_NODISCARD T* getUnsafe() const noexcept
    {
        uintptr_t self = uintptr_t(this);
        // dereferencing a NULL pointer is undefined behavior, so we can skip nullptr check
        ZMEYA_ASSERT(relativeOffset != 0);
        uintptr_t addr = toAbsoluteAddr(self, relativeOffset);
        return reinterpret_cast<T*>(addr);
    }

  public:
    Pointer() noexcept = default;
    // Pointer(const Pointer&) = delete;
    // Pointer& operator=(const Pointer&) = delete;

    ZMEYA_NODISCARD T* get() const noexcept
    {
        uintptr_t self = uintptr_t(this);
        uintptr_t addr = (relativeOffset == 0) ? uintptr_t(0) : toAbsoluteAddr(self, relativeOffset);
        return reinterpret_cast<T*>(addr);
    }

    // Note: implicit conversion operator
    // operator const T*() const noexcept { return get(); }
    // operator T*() noexcept { return get(); }

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

/*
    String
*/
class String
{
  public:
    Pointer<char> data;
    String() noexcept = default;
    // String(const String&) = delete;
    // String& operator=(const String&) = delete;

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
        // both strings can point to the same memory (fast-path)
        if (other.c_str() == c_str())
        {
            return true;
        }
        return isEqual(other.c_str());
    }
    ZMEYA_NODISCARD bool operator!=(const String& other) const noexcept
    {
        // both strings can point to the same memory (fast-path)
        if (other.c_str() == c_str())
        {
            return false;
        }
        return !isEqual(other.c_str());
    }

    // Assignment operators for automatic conversion
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

    // Friend declarations for new Builder API
    inline friend void assign(String& to, const std::string& from);
    inline friend void assign(String& to, const char* from);

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    void clear();
    String& append(const char* suf);
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

/*
    Array
*/
template <typename T> class Array
{
    roffset_t relativeOffset;
    uint32_t numElements;

  private:
    ZMEYA_NODISCARD const T* getConstData() const noexcept
    {
        uintptr_t addr = toAbsoluteAddr(uintptr_t(this), relativeOffset);
        return reinterpret_cast<const T*>(addr);
    }

    ZMEYA_NODISCARD T* getData() const noexcept { return const_cast<T*>(getConstData()); }

  public:
    Array() noexcept = default;
    // Array(const Array&) = delete;
    // Array& operator=(const Array&) = delete;

    ZMEYA_NODISCARD size_t size() const noexcept { return size_t(numElements); }

    ZMEYA_NODISCARD const T& operator[](const size_t index) const noexcept
    {
        const T* data = getConstData();
        return data[index];
    }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT

    ZMEYA_NODISCARD T* get_element_ptr_unsafe_can_be_relocated(const size_t index) noexcept
    {
        T* data = getData();
        return data + index;
    }

    ZMEYA_NODISCARD T* get_raw_ptr_unsafe_can_be_relocated() noexcept { return getData(); }
#endif

    ZMEYA_NODISCARD const T* at(const size_t index) const
    {
        ZMEYA_ASSERT(index < size());
        const T* data = getConstData();
        return data[index];
    }

    ZMEYA_NODISCARD const T* data() const noexcept { return getConstData(); }

    ZMEYA_NODISCARD const T* begin() const noexcept
    {
        const T* data = getConstData();
        return data;
    };

    ZMEYA_NODISCARD const T* end() const noexcept
    {
        const T* data = getConstData();
        return data + size();
    };

    ZMEYA_NODISCARD bool empty() const noexcept { return size() == 0; }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    template <typename U> void push_back(U&& v);
    void pop_back();
    void clear();
    void erase_at(size_t index);
    void resize(size_t new_size, const T& fill = T{});
#endif

    // Assignment operators for automatic conversion
    template<typename F>
    Array<T>& operator=(const std::vector<F>& other)
    {
        assign(*this, other);
        return *this;
    }

    // Friend declarations for new Builder API
    template <typename T, typename F> friend void assign(Array<T>& to, const std::vector<F>& from);
    template <typename Key, typename F> friend void assign(HashSet<Key>& to, const std::unordered_set<F>& from);
    template <typename Key, typename Value, typename FK, typename FV>
    friend void assign(HashMap<Key, Value>& to, const std::unordered_map<FK, FV>& from);
    template <typename T> friend void assign(Array<zm::Pointer<T>>& to, const std::vector<T*>& from);
    friend class detail::BuilderBase;
};

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
namespace detail
{
template <typename U> struct zm_array_push_back_ok : std::integral_constant<bool, std::is_trivially_copyable<U>::value>
{
};
template <> struct zm_array_push_back_ok<String> : std::false_type
{
};
template <typename P> struct zm_array_push_back_ok<Pointer<P>> : std::false_type
{
};
template <typename A> struct zm_array_push_back_ok<Array<A>> : std::false_type
{
};
} // namespace detail
#endif

/*

 Hash adapters

*/

// (key) generic adapter
template <typename Item> struct HashKeyAdapterGeneric
{
    typedef Item ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hasher(item); }
    static bool eq(const ItemType& a, const ItemType& b) { return a == b; }
};

// (key) adapter for std::string
struct HashKeyAdapterStdString
{
    typedef std::string ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hashString(item.c_str()); }
    static bool eq(const ItemType& a, const ItemType& b) { return a == b; }
};

// (key,value) generic adapter
template <typename Item> struct HashKeyValueAdapterGeneric
{
    typedef Item ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hasher(item.first); }
    static bool eq(const ItemType& a, const ItemType& b) { return a.first == b.first; }
};

// (key,value) adapter for std::string
template <typename Value> struct HashKeyValueAdapterStdString
{
    typedef std::pair<const std::string, Value> ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hashString(item.first.c_str()); }
    static bool eq(const ItemType& a, const ItemType& b) { return a.first == b.first; }
};

// (key) adapter for String and null-terminated c strings  >> SearchAdapterCStrToString
// used only for search
struct HashKeyAdapterCStr
{
    typedef const char* ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hashString(item); }
    static bool eq(const String& a, const ItemType& b) { return a == b; }
};

/*
    HashSet
*/
template <typename Key> class HashSet
{
  public:
    typedef Key Item;
    struct Bucket
    {
        uint32_t beginIndex;
        uint32_t endIndex;
    };
    Array<Bucket> buckets;
    Array<Item> items;

    template <typename Key2, typename Adapter> ZMEYA_NODISCARD bool containsImpl(const Key2& key) const noexcept
    {
        size_t numBuckets = buckets.size();
        if (numBuckets == 0)
        {
            return false;
        }
        size_t hashMod = numBuckets;
        size_t hash = Adapter::hash(key);
        size_t bucketIndex = hash % hashMod;
        const Bucket& bucket = buckets[bucketIndex];
        for (size_t i = bucket.beginIndex; i < bucket.endIndex; i++)
        {
            const Key& item = items[i];
            if (Adapter::eq(item, key))
            {
                return true;
            }
        }
        return false;
    }

  public:
    HashSet() noexcept = default;
    // HashSet(const HashSet&) = delete;
    // HashSet& operator=(const HashSet&) = delete;

    ZMEYA_NODISCARD size_t size() const noexcept { return items.size(); }

    ZMEYA_NODISCARD bool empty() const noexcept { return items.empty(); }

    ZMEYA_NODISCARD const Item* begin() const noexcept { return items.begin(); }

    ZMEYA_NODISCARD const Item* end() const noexcept { return items.end(); }

    ZMEYA_NODISCARD bool contains(const char* key) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        return containsImpl<const char*, HashKeyAdapterCStr>(key);
    }

    ZMEYA_NODISCARD bool contains(const Key& key) const noexcept { return containsImpl<Key, HashKeyAdapterGeneric<Key>>(key); }

    // Assignment operators for automatic conversion
    template<typename F>
    HashSet<Key>& operator=(const std::unordered_set<F>& other)
    {
        assign(*this, other);
        return *this;
    }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    template <typename F> void insert(const F& item);
    template <typename F> void erase(const F& key);
    void clear();
#endif

    // Friend declarations for new Builder API
    template <typename K, typename F> friend void assign(HashSet<K>& to, const std::unordered_set<F>& from);
};

/*
    Pair
*/
template <typename T1, typename T2> struct Pair
{
    T1 first;
    T2 second;

    Pair() noexcept = default;

    Pair(const T1& t1, const T2& t2) noexcept
        : first(t1)
        , second(t2)
    {
    }
};

/*
    HashMap
*/
template <typename Key, typename Value> class HashMap
{
    typedef Pair<const Key, Value> Item;
    struct Bucket
    {
        uint32_t beginIndex;
        uint32_t endIndex;
    };
    Array<Bucket> buckets;
    Array<Item> items;

    friend class detail::BuilderBase;

    template <typename Adapter, typename Key2> ZMEYA_NODISCARD const Value* findImpl(const Key2& key) const noexcept
    {
        size_t numBuckets = buckets.size();
        if (numBuckets == 0)
        {
            return nullptr;
        }
        size_t hashMod = numBuckets;
        size_t hash = Adapter::hash(key);
        size_t bucketIndex = hash % hashMod;
        const Bucket& bucket = buckets[bucketIndex];
        for (size_t i = bucket.beginIndex; i < bucket.endIndex; i++)
        {
            const Item& item = items[i];
            if (Adapter::eq(item.first, key))
            {
                return &item.second;
            }
        }
        return nullptr;
    }

  public:
    HashMap() noexcept = default;
    // HashMap(const HashMap&) = delete;
    // HashMap& operator=(const HashMap&) = delete;

    ZMEYA_NODISCARD size_t size() const noexcept { return items.size(); }

    ZMEYA_NODISCARD bool empty() const noexcept { return items.empty(); }

    ZMEYA_NODISCARD const Item* begin() const noexcept { return items.begin(); }

    ZMEYA_NODISCARD const Item* end() const noexcept { return items.end(); }

    ZMEYA_NODISCARD bool contains(const Key& key) const noexcept { return find(key) != nullptr; }

    ZMEYA_NODISCARD Value* find(const Key& key) noexcept
    {
        typedef HashKeyAdapterGeneric<Key> Adapter;

        const Value* res = findImpl<Adapter>(key);
        return res ? const_cast<Value*>(res) : nullptr;
    }
    ZMEYA_NODISCARD const Value* find(const Key& key) const noexcept
    {
        typedef HashKeyAdapterGeneric<Key> Adapter;

        const Value* res = findImpl<Adapter>(key);
        return res;
    }

    ZMEYA_NODISCARD const Value& find(const Key& key, const Value& valueIfNotFound) const noexcept
    {
        typedef HashKeyAdapterGeneric<Key> Adapter;

        const Value* res = findImpl<Adapter>(key);
        if (res)
        {
            return *res;
        }
        return valueIfNotFound;
    }

    ZMEYA_NODISCARD bool contains(const char* key) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        return find(key) != nullptr;
    }

    ZMEYA_NODISCARD Value* find(const char* key) noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        typedef HashKeyAdapterCStr Adapter;

        const Value* res = findImpl<Adapter>(key);
        return res ? const_cast<Value*>(res) : nullptr;
    }
    ZMEYA_NODISCARD const Value* find(const char* key) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        typedef HashKeyAdapterCStr Adapter;

        const Value* res = findImpl<Adapter>(key);
        return res;
    }

    ZMEYA_NODISCARD const Value& find(const char* key, const Value& valueIfNotFound) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        typedef HashKeyAdapterCStr Adapter;

        const Value* res = findImpl<Adapter>(key);
        if (res)
        {
            return *res;
        }
        return valueIfNotFound;
    }

    ZMEYA_NODISCARD const char* find(const Key& key, const char* valueIfNotFound) const noexcept
    {
        static_assert(std::is_same<Value, String>::value, "To use this function, the value type must be Zmeya::String");
        typedef HashKeyAdapterGeneric<Key> Adapter;

        const Value* res = findImpl<Adapter>(key);
        if (res)
        {
            return res->c_str();
        }
        return valueIfNotFound;
    }

    ZMEYA_NODISCARD const char* find(const char* key, const char* valueIfNotFound) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        static_assert(std::is_same<Value, String>::value, "To use this function, the value type must be Zmeya::String");
        typedef HashKeyAdapterCStr Adapter;

        const Value* res = findImpl<Adapter>(key);
        if (res)
        {
            return res->c_str();
        }
        return valueIfNotFound;
    }

    // Assignment operators for automatic conversion
    template<typename FK, typename FV>
    HashMap<Key, Value>& operator=(const std::unordered_map<FK, FV>& other)
    {
        assign(*this, other);
        return *this;
    }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    template <typename FK, typename FV> void insert(const FK& key, const FV& value);
    template <typename FK> void erase(const FK& key);
    void clear();
#endif

    // Friend declarations for new Builder API
    template <typename K, typename V, typename FK, typename FV>
    friend void assign(HashMap<K, V>& to, const std::unordered_map<FK, FV>& from);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT

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
    Aligned allocator for the detail builder backing buffer.
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
        return static_cast<pointer>(pv);
    }

    void deallocate(pointer p, size_type)
    {
        //
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

    Span

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

**Serialization backing types**

**`goffset_t`** is defined before **`zm::detail`** builder types that use it.

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

/*

**Default write arena capacity**

`std::vector<char>` for the blob writer starts with a modest reserve so typical small blobs avoid an immediate growth step.
This is not a correctness knob: user code must not cache raw pointers into the arena across operations that can grow the buffer.

*/

inline constexpr size_t kDefaultWriteBlobArenaReserveBytes = size_t(64) * 1024;

inline thread_local BuilderBase* g_tls_active_builder = nullptr;

inline BuilderBase* get_global_builder() noexcept { return g_tls_active_builder; }

inline void set_global_builder(BuilderBase* builder) noexcept { g_tls_active_builder = builder; }

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
        prev = get_global_builder();
        set_global_builder(builder);
    }

    ~ScopedBuilder() { set_global_builder(prev); }

    ScopedBuilder(const ScopedBuilder&) = delete;
    ScopedBuilder& operator=(const ScopedBuilder&) = delete;

  private:
    BuilderBase* prev;
};

class BuilderBase
{
  public:
    std::vector<char, BufferAllocator<char, ZMEYA_MAX_ALIGN>> data;

    struct PrivateToken
    {
    };

    std::unordered_map<goffset_t, goffset_t> roffset_slot_targets_;

    std::unordered_map<goffset_t, size_t> string_blob_phys_bytes_;

    std::vector<std::pair<goffset_t, size_t>> dead_ranges_;

    void note_dead_range(goffset_t start, size_t len)
    {
        if (len == 0 || data.empty())
        {
            return;
        }
        ZMEYA_ASSERT(start >= goffset_t(0));
        ZMEYA_ASSERT(size_t(start) < data.size());
        ZMEYA_ASSERT(len <= data.size());
        ZMEYA_ASSERT(size_t(start) <= data.size() - len);
        dead_ranges_.push_back(std::pair<goffset_t, size_t>(start, len));
    }

    static bool goffset_in_any_merged_dead_range(goffset_t g, const std::vector<std::pair<goffset_t, size_t>>& merged)
    {
        if (g == goffset_t(0))
        {
            return false;
        }
        for (const auto& d : merged)
        {
            const goffset_t dEnd = d.first + goffset_t(d.second);
            if (g >= d.first && g < dEnd)
            {
                return true;
            }
        }
        return false;
    }

    void prune_roffset_registry_overlapping_merged_dead(const std::vector<std::pair<goffset_t, size_t>>& merged)
    {
        for (auto it = roffset_slot_targets_.begin(); it != roffset_slot_targets_.end();)
        {
            const goffset_t sg = it->first;
            const goffset_t tg = it->second;
            if (goffset_in_any_merged_dead_range(sg, merged) || goffset_in_any_merged_dead_range(tg, merged))
            {
                it = roffset_slot_targets_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    static void merge_intervals(std::vector<std::pair<goffset_t, size_t>>& iv)
    {
        if (iv.empty())
        {
            return;
        }
        std::sort(iv.begin(), iv.end(), [](const std::pair<goffset_t, size_t>& a, const std::pair<goffset_t, size_t>& b) { return a.first < b.first; });
        std::vector<std::pair<goffset_t, size_t>> out;
        out.reserve(iv.size());
        goffset_t curS = iv[0].first;
        size_t curE = iv[0].second;
        for (size_t i = 1; i < iv.size(); ++i)
        {
            goffset_t s = iv[i].first;
            size_t len = iv[i].second;
            goffset_t curEnd = curS + goffset_t(curE);
            if (s <= curEnd)
            {
                goffset_t newEnd = (std::max)(curEnd, s + goffset_t(len));
                curE = size_t(newEnd - curS);
            }
            else
            {
                out.push_back(std::pair<goffset_t, size_t>(curS, curE));
                curS = s;
                curE = len;
            }
        }
        out.push_back(std::pair<goffset_t, size_t>(curS, curE));
        iv.swap(out);
    }

#if defined(ZMEYA_BUILDER_PARANOID)
    void debug_validate_roffset_registry_before_patch() const
    {
        const size_t n = data.size();
        for (const auto& kv : roffset_slot_targets_)
        {
            ZMEYA_ASSERT(kv.first >= goffset_t(0));
            ZMEYA_ASSERT(size_t(kv.first) + sizeof(roffset_t) <= n);
            if (kv.second != goffset_t(0))
            {
                ZMEYA_ASSERT(kv.second >= goffset_t(0));
                ZMEYA_ASSERT(size_t(kv.second) < n);
            }
        }
    }
#endif

    void compact_arena_and_remap_registry()
    {
        if (dead_ranges_.empty())
        {
            return;
        }

        merge_intervals(dead_ranges_);
        prune_roffset_registry_overlapping_merged_dead(dead_ranges_);

        const size_t oldSize = data.size();
        goffset_t prevDeadEnd = goffset_t(0);
        for (const auto& d : dead_ranges_)
        {
            ZMEYA_ASSERT(d.first >= goffset_t(0));
            ZMEYA_ASSERT(d.second > 0);
            ZMEYA_ASSERT(size_t(d.first) < oldSize);
            ZMEYA_ASSERT(d.second <= oldSize);
            ZMEYA_ASSERT(size_t(d.first) <= oldSize - d.second);
            ZMEYA_ASSERT(d.first >= prevDeadEnd);
            prevDeadEnd = d.first + goffset_t(d.second);
            ZMEYA_ASSERT(prevDeadEnd <= goffset_t(oldSize));
        }

        goffset_t scan = 0;
        std::vector<char, BufferAllocator<char, ZMEYA_MAX_ALIGN>> newData;
        struct Piece
        {
            goffset_t old_lo;
            goffset_t old_hi;
            goffset_t new_lo;
        };
        std::vector<Piece> pieces;
        pieces.reserve(dead_ranges_.size() + 1);

        goffset_t newCursor = 0;
        for (const auto& d : dead_ranges_)
        {
            goffset_t dStart = d.first;
            size_t dLen = d.second;
            if (scan < dStart)
            {
                size_t runLen = size_t(dStart - scan);
                newData.resize(newData.size() + runLen);
                std::memcpy(newData.data() + size_t(newCursor), data.data() + size_t(scan), runLen);
                pieces.push_back(Piece{scan, dStart, newCursor});
                newCursor += goffset_t(runLen);
            }
            scan = dStart + goffset_t(dLen);
        }
        if (scan < goffset_t(oldSize))
        {
            size_t runLen = size_t(goffset_t(oldSize) - scan);
            newData.resize(newData.size() + runLen);
            std::memcpy(newData.data() + size_t(newCursor), data.data() + size_t(scan), runLen);
            pieces.push_back(Piece{scan, goffset_t(oldSize), newCursor});
            newCursor += goffset_t(runLen);
            scan = goffset_t(oldSize);
        }
        ZMEYA_ASSERT(scan == goffset_t(oldSize));
        ZMEYA_ASSERT(newData.size() <= oldSize);
        ZMEYA_ASSERT(size_t(newCursor) == newData.size());

        auto remap_one = [&](goffset_t oldG) -> goffset_t {
            ZMEYA_ASSERT(!pieces.empty());
            auto it = std::upper_bound(pieces.begin(), pieces.end(), oldG, [](goffset_t g, const Piece& p) { return g < p.old_lo; });
            ZMEYA_ASSERT(it != pieces.begin());
            --it;
            ZMEYA_ASSERT(oldG >= it->old_lo && oldG < it->old_hi);
            return it->new_lo + (oldG - it->old_lo);
        };

        std::unordered_map<goffset_t, goffset_t> newMap;
        newMap.reserve(roffset_slot_targets_.size() + 8);
        for (const auto& kv : roffset_slot_targets_)
        {
            goffset_t nk = remap_one(kv.first);
            goffset_t nt = (kv.second == goffset_t(0)) ? goffset_t(0) : remap_one(kv.second);
            newMap[nk] = nt;
        }
        roffset_slot_targets_ = std::move(newMap);

        data.swap(newData);
        dead_ranges_.clear();
        string_blob_phys_bytes_.clear();
    }

    void register_roffset_slot(goffset_t slot_field_goffset, goffset_t target_goffset)
    {
        ZMEYA_ASSERT(!data.empty());
        ZMEYA_ASSERT(slot_field_goffset >= goffset_t(0));
        ZMEYA_ASSERT(size_t(slot_field_goffset) + sizeof(roffset_t) <= data.size());
        if (target_goffset != goffset_t(0))
        {
            ZMEYA_ASSERT(target_goffset >= goffset_t(0));
            ZMEYA_ASSERT(size_t(target_goffset) < data.size());
        }
        roffset_slot_targets_[slot_field_goffset] = target_goffset;
    }

    void string_blob_phys_register(goffset_t blob_start, size_t nbytes)
    {
        string_blob_phys_bytes_[blob_start] = nbytes;
    }

    void patch_roffset_slots_from_registry()
    {
#if defined(ZMEYA_BUILDER_PARANOID)
        debug_validate_roffset_registry_before_patch();
#endif
        for (const auto& kv : roffset_slot_targets_)
        {
            goffset_t slot_g = kv.first;
            goffset_t target_g = kv.second;
            ZMEYA_ASSERT(size_t(slot_g) + sizeof(roffset_t) <= data.size());
            roffset_t* pr = reinterpret_cast<roffset_t*>(&data[slot_g]);
            if (target_g == 0)
            {
                *pr = 0;
            }
            else
            {
                const void* slotPtr = &data[slot_g];
                goffset_t baseG = get_global_offset(slotPtr);
                diff_t d = diff_t(goffset_t(target_g)) - diff_t(goffset_t(baseG));
                ZMEYA_ASSERT(d >= diff_t(std::numeric_limits<roffset_t>::min()));
                ZMEYA_ASSERT(d <= diff_t(std::numeric_limits<roffset_t>::max()));
                *pr = roffset_t(d);
            }
        }
    }

    goffset_t alloc_aligned(size_t numBytes, size_t alignment)
    {
        ZMEYA_ASSERT(isPowerOfTwo(alignment));
        ZMEYA_ASSERT(alignment <= ZMEYA_MAX_ALIGN);

        size_t cursor = data.size();

        // Calculate padding for alignment
        size_t off = cursor & (alignment - 1);
        size_t padding = (off != 0) ? (alignment - off) : 0;
        ZMEYA_ASSERT(padding <= SIZE_MAX - cursor);
        size_t allocOffset = cursor + padding;
        ZMEYA_ASSERT(numBytes <= SIZE_MAX - allocOffset);
        size_t totalBytes = numBytes + padding;
        ZMEYA_ASSERT(totalBytes >= numBytes);

        // Resize with zero-initialization
        ZMEYA_ASSERT(cursor <= SIZE_MAX - totalBytes);
        data.resize(cursor + totalBytes, char(0));

        // Verify alignment
        ZMEYA_ASSERT((uintptr_t(&data[allocOffset]) & (alignment - 1)) == 0);

        // check that global offset fit
        ZMEYA_ASSERT((allocOffset >= 0 && allocOffset < std::numeric_limits<goffset_t>::max()) && "Offset is too big, more that 2GB?");
        return goffset_t(allocOffset);
    }

    template <typename T, typename... _Valty> static void placementCtor(void* ptr, _Valty&&... _Val)
    {
        ::new (const_cast<void*>(static_cast<const volatile void*>(ptr))) T(std::forward<_Valty>(_Val)...);
    }

    void* get_ptr_unsafe_to_store(goffset_t g_offs)
    {
        ZMEYA_ASSERT(!data.empty());
        ZMEYA_ASSERT(g_offs >= goffset_t(0));
        ZMEYA_ASSERT(size_t(g_offs) < data.size());
        return &data[size_t(g_offs)];
    }

    template <typename T> T* allocate()
    {
        static_assert(std::is_trivially_copyable<T>::value, "Only trivially copyable types allowed");
        goffset_t g_offs = alloc_aligned(sizeof(T), alignof(T));
        void* ptr = get_ptr_unsafe_to_store(g_offs);
        BuilderBase::placementCtor<T>(ptr);
        return reinterpret_cast<T*>(ptr);
    }

    template <typename T> void note_dead_array_slab_if_any(Array<T>* arr_hdr)
    {
        if (arr_hdr->numElements == 0 && arr_hdr->relativeOffset == 0)
        {
            return;
        }
        const size_t n = size_t(arr_hdr->numElements);
        ZMEYA_ASSERT(n == 0 || sizeof(T) <= SIZE_MAX / n);
        T* pdata = arr_hdr->get_raw_ptr_unsafe_can_be_relocated();
        note_dead_range(get_global_offset(pdata), n * sizeof(T));
    }

    template <typename Key> void note_dead_hashset_if_any(HashSet<Key>* hs)
    {
        note_dead_array_slab_if_any<typename HashSet<Key>::Bucket>(&hs->buckets);
        note_dead_array_slab_if_any<Key>(&hs->items);
    }

    template <typename K, typename V> void note_dead_hashmap_if_any(HashMap<K, V>* hm)
    {
        note_dead_array_slab_if_any<typename HashMap<K, V>::Bucket>(&hm->buckets);
        using ItemType = Pair<const K, V>;
        note_dead_array_slab_if_any<ItemType>(&hm->items);
    }

    // Helper methods for assignment functions
    bool contains_pointer(const void* ptr) const
    {
        if (data.empty())
        {
            return false;
        }
        const char* dataStart = data.data();
        const char* dataEnd = dataStart + data.size();
        return (ptr >= dataStart && ptr < dataEnd);
    }

    // TODO: rename maybe?
    goffset_t get_global_offset(const void* ptr) const
    {
        ZMEYA_ASSERT(contains_pointer(ptr));
        offset_t allocOffset = offset_t(uintptr_t(ptr) - uintptr_t(data.data()));
        // check that global offset fit
        ZMEYA_ASSERT((allocOffset >= 0 && allocOffset < std::numeric_limits<goffset_t>::max()) && "Offset is too big, more that 2GB?");
        return goffset_t(allocOffset);
    }

    explicit BuilderBase(size_t initialSizeInBytes, PrivateToken)
    {
        // 16 bytes minimum
        size_t reserveSize = std::max(initialSizeInBytes, size_t(16));
        data.reserve(reserveSize);

        // Static assertions for zm types
        static_assert(std::is_trivially_copyable<Pointer<int>>::value, "Pointer is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<Array<int>>::value, "Array is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<HashSet<int>>::value, "HashSet is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<Pair<int, float>>::value, "Pair is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<HashMap<int, int>>::value, "HashMap is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<String>::value, "String is_trivially_copyable check failed");
    }

    ~BuilderBase() = default;

    // Non-copyable, non-movable to keep it simple
    BuilderBase(const BuilderBase&) = delete;
    BuilderBase& operator=(const BuilderBase&) = delete;
    BuilderBase(BuilderBase&&) = delete;
    BuilderBase& operator=(BuilderBase&&) = delete;

    void string_note_dead_existing_blob(String& s)
    {
        goffset_t str_g = get_global_offset(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        const char* old_ptr = slot->data.get();
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            goffset_t blob_g = get_global_offset(old_ptr);
            size_t logical_len = std::strlen(old_ptr);
            size_t phys = logical_len + 1;
            auto it = string_blob_phys_bytes_.find(blob_g);
            if (it != string_blob_phys_bytes_.end())
            {
                phys = it->second;
                string_blob_phys_bytes_.erase(it);
            }
            note_dead_range(blob_g, phys);
        }
    }

    void string_append_cstr(String& s, const char* suf, size_t suf_len)
    {
        if (suf_len == 0)
        {
            return;
        }
        ZMEYA_ASSERT(suf != nullptr);
        goffset_t str_g = get_global_offset(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        size_t old_len = 0;
        goffset_t old_blob_g = goffset_t(0);
        const char* old_ptr = slot->data.get();
        size_t old_phys_for_growth = 0;
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            old_blob_g = get_global_offset(old_ptr);
            old_len = std::strlen(old_ptr);
            old_phys_for_growth = old_len + 1;
            auto pit = string_blob_phys_bytes_.find(old_blob_g);
            if (pit != string_blob_phys_bytes_.end())
            {
                old_phys_for_growth = pit->second;
                string_blob_phys_bytes_.erase(pit);
            }
            note_dead_range(old_blob_g, old_phys_for_growth);
        }
        ZMEYA_ASSERT(suf_len <= SIZE_MAX - old_len);
        ZMEYA_ASSERT(old_len + suf_len <= SIZE_MAX - 1);
        const size_t new_len = old_len + suf_len;
        size_t alloc_bytes = new_len + 1;
        if (old_ptr != nullptr)
        {
            if (old_phys_for_growth >= alloc_bytes)
            {
                alloc_bytes = old_phys_for_growth;
            }
            else
            {
                size_t cap = (std::max)(old_phys_for_growth, size_t(8));
                while (cap < alloc_bytes && cap <= SIZE_MAX / 2)
                {
                    cap *= 2;
                }
                if (cap < alloc_bytes)
                {
                    cap = alloc_bytes;
                }
                alloc_bytes = cap;
            }
        }
        goffset_t blob_g = alloc_aligned(alloc_bytes, 1);
        slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        char* dest = reinterpret_cast<char*>(get_ptr_unsafe_to_store(blob_g));
        if (old_len != 0)
        {
            const char* old_src = reinterpret_cast<const char*>(get_ptr_unsafe_to_store(old_blob_g));
            std::memcpy(dest, old_src, old_len);
        }
        std::memcpy(dest + old_len, suf, suf_len);
        dest[new_len] = '\0';
        slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        goffset_t ptr_slot_g = get_global_offset(&slot->data);
        slot->data.relativeOffset = get_relative_offset(&slot->data, blob_g);
        register_roffset_slot(ptr_slot_g, blob_g);
        string_blob_phys_bytes_[blob_g] = alloc_bytes;
    }

    void string_clear(String& s)
    {
        goffset_t str_g = get_global_offset(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        const char* old_ptr = slot->data.get();
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            goffset_t blob_g = get_global_offset(old_ptr);
            size_t phys = std::strlen(old_ptr) + 1;
            auto it = string_blob_phys_bytes_.find(blob_g);
            if (it != string_blob_phys_bytes_.end())
            {
                phys = it->second;
                string_blob_phys_bytes_.erase(it);
            }
            note_dead_range(blob_g, phys);
        }
        slot->data.relativeOffset = 0;
        register_roffset_slot(get_global_offset(&slot->data), goffset_t(0));
    }

    template <typename T, typename U> void array_push_back(Array<T>& arr, U&& value)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value,
            "array_push_back is only supported for slab-memcpy-safe element types; use assign(vector<...>) for String, Pointer, nested Array, etc.");
        goffset_t hdr_g = get_global_offset(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        size_t old_n = size_t(slot->numElements);
        size_t new_n = old_n + 1;
        constexpr size_t alignOfT = std::alignment_of<T>::value;
        constexpr size_t sizeOfT = sizeof(T);
        size_t new_cap = new_n;
        if (old_n == 0)
        {
            new_cap = (std::max)(size_t(8), new_n);
        }
        else
        {
            ZMEYA_ASSERT(old_n <= SIZE_MAX / 2);
            new_cap = (std::max)(new_n, old_n * 2);
        }
        ZMEYA_ASSERT(new_cap <= SIZE_MAX / sizeOfT);
        if (old_n > 0)
        {
            T* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
            note_dead_range(get_global_offset(old_data), old_n * sizeOfT);
        }
        goffset_t new_blob_g = alloc_aligned(sizeOfT * new_cap, alignOfT);
        slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        T* new_data = reinterpret_cast<T*>(get_ptr_unsafe_to_store(new_blob_g));
        if (old_n > 0)
        {
            T* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
            std::memcpy(new_data, old_data, old_n * sizeOfT);
        }
        placementCtor<T>(&new_data[old_n], std::forward<U>(value));
        slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        slot->numElements = uint32_t(new_n);
        slot->relativeOffset = get_relative_offset(slot, new_blob_g);
        register_roffset_slot(hdr_g, new_blob_g);
    }

    template <typename T> void array_pop_back(Array<T>& arr)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value, "array_pop_back requires same constraints as push_back for this builder");
        goffset_t hdr_g = get_global_offset(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        if (slot->numElements == 0)
        {
            return;
        }
        slot->numElements -= 1;
    }

    template <typename T> void array_clear(Array<T>& arr)
    {
        goffset_t hdr_g = get_global_offset(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        note_dead_array_slab_if_any(slot);
        slot->numElements = 0;
        slot->relativeOffset = 0;
        register_roffset_slot(hdr_g, goffset_t(0));
    }

    template <typename T> void array_erase_at(Array<T>& arr, size_t index)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value, "array_erase_at requires same constraints as push_back for this builder");
        goffset_t hdr_g = get_global_offset(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        if (index >= size_t(slot->numElements))
        {
            return;
        }
        T* pdata = slot->get_raw_ptr_unsafe_can_be_relocated();
        constexpr size_t sizeOfT = sizeof(T);
        size_t tail = size_t(slot->numElements) - index - 1;
        if (tail > 0)
        {
            std::memmove(reinterpret_cast<char*>(pdata + index), reinterpret_cast<char*>(pdata + index + 1), tail * sizeOfT);
        }
        slot->numElements -= 1;
    }

    template <typename T> void array_resize_fill(Array<T>& arr, size_t new_size, const T& fill)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value, "array_resize_fill requires same constraints as push_back for this builder");
        goffset_t hdr_g = get_global_offset(&arr);
        for (;;)
        {
            Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
            size_t old_n = size_t(slot->numElements);
            if (new_size == old_n)
            {
                return;
            }
            if (new_size < old_n)
            {
                slot->numElements = uint32_t(new_size);
                return;
            }
            array_push_back(*slot, fill);
        }
    }

    template <typename T> void impl_assign_pointer_array(zm::Array<zm::Pointer<T>>& _to, const std::vector<T*>& from);

    template <typename T, typename F> void impl_assign_array_vector(zm::Array<T>& _to, const std::vector<F>& from);

    template <typename Key, typename F> void impl_assign_hashset(zm::HashSet<Key>& _to, const std::unordered_set<F>& from);

    template <typename Key, typename Value, typename FK, typename FV>
    void impl_assign_hashmap(zm::HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from);

    /*
    **finalize**

    Phase 0: compact bump arena and remap roffset registry targets.
    Phase 1: grow padding to alignment (layout bytes in-place).
    Phase 2: patch every registered self-relative word from parallel goffset targets (Q9).
    */

    Span<char> finalize(size_t alignment = 4)
    {
        ZMEYA_ASSERT(alignment > 0);
        ZMEYA_ASSERT(isPowerOfTwo(alignment));
        compact_arena_and_remap_registry();
        ZMEYA_ASSERT(dead_ranges_.empty());

        size_t currentSize = data.size();
        size_t remainder = currentSize % alignment;
        if (remainder != 0)
        {
            size_t paddingNeeded = alignment - remainder;
            ZMEYA_ASSERT(paddingNeeded <= SIZE_MAX - currentSize);
            data.resize(currentSize + paddingNeeded, char(0));
        }

        ZMEYA_ASSERT((data.size() % alignment) == 0);

        patch_roffset_slots_from_registry();

        return Span<char>(data.data(), data.size());
    }

    template <typename T> roffset_t get_relative_offset(const T* base, goffset_t ofs) const
    {
        ZMEYA_ASSERT(is_stack_pointer(base) == false && "Stack pointer detected!");
        ZMEYA_ASSERT(contains_pointer(base) && "A pointer should belong to the builder");
        ZMEYA_ASSERT(ofs >= goffset_t(0));
        ZMEYA_ASSERT(size_t(ofs) < data.size());

        goffset_t baseOffset = get_global_offset(base);
        diff_t diff = ofs - baseOffset;

        ZMEYA_ASSERT(diff >= diff_t(std::numeric_limits<roffset_t>::min()));
        ZMEYA_ASSERT(diff <= diff_t(std::numeric_limits<roffset_t>::max()));
        return roffset_t(diff);
    }


};

template <typename T> void BuilderBase::impl_assign_pointer_array(zm::Array<zm::Pointer<T>>& _to, const std::vector<T*>& from)
{
    goffset_t to_offset = get_global_offset(&_to);
    if (from.empty())
    {
        zm::Array<zm::Pointer<T>>* to = reinterpret_cast<zm::Array<zm::Pointer<T>>*>(get_ptr_unsafe_to_store(to_offset));
        if (to->numElements != 0 || to->relativeOffset != 0)
        {
            note_dead_array_slab_if_any<zm::Pointer<T>>(to);
            to->numElements = 0;
            to->relativeOffset = 0;
            register_roffset_slot(to_offset, goffset_t(0));
        }
        return;
    }
    zm::Array<zm::Pointer<T>>* to = reinterpret_cast<zm::Array<zm::Pointer<T>>*>(get_ptr_unsafe_to_store(to_offset));
    if (to->numElements != 0 || to->relativeOffset != 0)
    {
        note_dead_array_slab_if_any<zm::Pointer<T>>(to);
        to->numElements = 0;
        to->relativeOffset = 0;
        register_roffset_slot(to_offset, goffset_t(0));
        to = reinterpret_cast<zm::Array<zm::Pointer<T>>*>(get_ptr_unsafe_to_store(to_offset));
    }
    constexpr size_t alignOfPtr = std::alignment_of<zm::Pointer<T>>::value;
    constexpr size_t sizeOfPtr = sizeof(zm::Pointer<T>);
    zm::goffset_t arrayDataOffset = alloc_aligned(sizeOfPtr * from.size(), alignOfPtr);
    to = reinterpret_cast<zm::Array<zm::Pointer<T>>*>(get_ptr_unsafe_to_store(to_offset));
    to->numElements = uint32_t(from.size());
    to->relativeOffset = get_relative_offset(to, arrayDataOffset);
    register_roffset_slot(to_offset, arrayDataOffset);
    for (size_t i = 0; i < from.size(); ++i)
    {
        zm::goffset_t elementOffset = zm::goffset_t(arrayDataOffset + i * sizeOfPtr);
        zm::Pointer<T>* element = reinterpret_cast<zm::Pointer<T>*>(get_ptr_unsafe_to_store(elementOffset));
        BuilderBase::placementCtor<zm::Pointer<T>>(element);
        assign_pointer(*this, *element, from[i]);
    }
}

template <typename T, typename F> void BuilderBase::impl_assign_array_vector(zm::Array<T>& _to, const std::vector<F>& from)
{
    goffset_t to_offset = get_global_offset(&_to);
    if (from.empty())
    {
        zm::Array<T>* to = reinterpret_cast<zm::Array<T>*>(get_ptr_unsafe_to_store(to_offset));
        if (to->numElements != 0 || to->relativeOffset != 0)
        {
            note_dead_array_slab_if_any<T>(to);
            to->numElements = 0;
            to->relativeOffset = 0;
            register_roffset_slot(to_offset, goffset_t(0));
        }
        return;
    }
    zm::Array<T>* to = reinterpret_cast<zm::Array<T>*>(get_ptr_unsafe_to_store(to_offset));
    if (to->numElements != 0 || to->relativeOffset != 0)
    {
        note_dead_array_slab_if_any<T>(to);
        to->numElements = 0;
        to->relativeOffset = 0;
        register_roffset_slot(to_offset, goffset_t(0));
        to = reinterpret_cast<zm::Array<T>*>(get_ptr_unsafe_to_store(to_offset));
    }
    constexpr size_t alignOfT = std::alignment_of<T>::value;
    constexpr size_t sizeOfT = sizeof(T);
    zm::goffset_t arrayDataOffset = alloc_aligned(sizeOfT * from.size(), alignOfT);
    to = reinterpret_cast<zm::Array<T>*>(get_ptr_unsafe_to_store(to_offset));
    to->numElements = uint32_t(from.size());
    to->relativeOffset = get_relative_offset(to, arrayDataOffset);
    register_roffset_slot(to_offset, arrayDataOffset);
    for (size_t i = 0; i < from.size(); ++i)
    {
        zm::goffset_t elementOffset = zm::goffset_t(arrayDataOffset + i * sizeOfT);
        zm::deep_copy<F, T>(*this, from[i], elementOffset);
    }
}

template <typename Key, typename F> void BuilderBase::impl_assign_hashset(zm::HashSet<Key>& _to, const std::unordered_set<F>& from)
{
    ScopedBuilder tls(this);
    goffset_t to_offset = get_global_offset(&_to);
    zm::HashSet<Key>* to = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(to_offset));
    note_dead_hashset_if_any<Key>(to);
    if (from.empty())
    {
        to->buckets.numElements = 0;
        to->buckets.relativeOffset = 0;
        to->items.numElements = 0;
        to->items.relativeOffset = 0;
        register_roffset_slot(get_global_offset(&to->buckets), goffset_t(0));
        register_roffset_slot(get_global_offset(&to->items), goffset_t(0));
        return;
    }

    size_t numElements = from.size();
    size_t numBuckets = numElements * 2;
    ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
    size_t hashMod = numBuckets;

    constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashSet<Key>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename zm::HashSet<Key>::Bucket);
    zm::goffset_t bucketsDataOffset = alloc_aligned(sizeOfBucket * numBuckets, alignOfBucket);

    typename zm::HashSet<Key>::Bucket* buckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
    for (size_t i = 0; i < numBuckets; ++i)
    {
        BuilderBase::placementCtor<typename zm::HashSet<Key>::Bucket>(&buckets[i], typename zm::HashSet<Key>::Bucket{0, 0});
    }

    std::vector<size_t> item_hashes;
    item_hashes.reserve(from.size());
    for (const auto& item : from)
    {
        size_t h;
        if constexpr (std::is_same_v<F, std::string>)
        {
            h = HashUtils::hashString(item.c_str());
        }
        else
        {
            h = HashUtils::hasher(item);
        }
        item_hashes.push_back(h);
    }
    for (size_t hi = 0; hi < item_hashes.size(); ++hi)
    {
        size_t bucketIndex = item_hashes[hi] % hashMod;
        buckets[bucketIndex].beginIndex++;
    }

    size_t beginIndex = 0;
    for (size_t bucketIndex = 0; bucketIndex < numBuckets; bucketIndex++)
    {
        typename zm::HashSet<Key>::Bucket& bucket = buckets[bucketIndex];
        size_t numElementsInBucket = bucket.beginIndex;
        bucket.beginIndex = uint32_t(beginIndex);
        bucket.endIndex = bucket.beginIndex;
        beginIndex += numElementsInBucket;
    }

    constexpr size_t alignOfKey = std::alignment_of<Key>::value;
    constexpr size_t sizeOfKey = sizeof(Key);
    zm::goffset_t itemsDataOffset = alloc_aligned(sizeOfKey * numElements, alignOfKey);

    Key* items = reinterpret_cast<Key*>(get_ptr_unsafe_to_store(itemsDataOffset));
    buckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));

    size_t hi = 0;
    for (const auto& item : from)
    {
        size_t bucketIndex = item_hashes[hi++] % hashMod;
        typename zm::HashSet<Key>::Bucket& bucket = buckets[bucketIndex];

        Key* element = &items[bucket.endIndex];
        BuilderBase::placementCtor<Key>(element);

        *element = item;

        bucket.endIndex++;
    }

    to = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(to_offset));
    to->buckets.numElements = uint32_t(numBuckets);
    to->buckets.relativeOffset = get_relative_offset(&to->buckets, bucketsDataOffset);
    to->items.numElements = uint32_t(numElements);
    to->items.relativeOffset = get_relative_offset(&to->items, itemsDataOffset);
    register_roffset_slot(get_global_offset(&to->buckets), bucketsDataOffset);
    register_roffset_slot(get_global_offset(&to->items), itemsDataOffset);
}

template <typename Key, typename Value, typename FK, typename FV>
void BuilderBase::impl_assign_hashmap(zm::HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from)
{
    ScopedBuilder tls(this);
    goffset_t to_offset = get_global_offset(&_to);
    zm::HashMap<Key, Value>* to = reinterpret_cast<zm::HashMap<Key, Value>*>(get_ptr_unsafe_to_store(to_offset));
    note_dead_hashmap_if_any<Key, Value>(to);
    if (from.empty())
    {
        to->buckets.numElements = 0;
        to->buckets.relativeOffset = 0;
        to->items.numElements = 0;
        to->items.relativeOffset = 0;
        register_roffset_slot(get_global_offset(&to->buckets), goffset_t(0));
        register_roffset_slot(get_global_offset(&to->items), goffset_t(0));
        return;
    }

    size_t numElements = from.size();
    size_t numBuckets = numElements * 2;
    ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
    size_t hashMod = numBuckets;

    constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashMap<Key, Value>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename zm::HashMap<Key, Value>::Bucket);
    zm::goffset_t bucketsDataOffset = alloc_aligned(sizeOfBucket * numBuckets, alignOfBucket);

    typename zm::HashMap<Key, Value>::Bucket* buckets = reinterpret_cast<typename zm::HashMap<Key, Value>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
    for (size_t i = 0; i < numBuckets; ++i)
    {
        BuilderBase::placementCtor<typename zm::HashMap<Key, Value>::Bucket>(&buckets[i], typename zm::HashMap<Key, Value>::Bucket{0, 0});
    }

    std::vector<size_t> key_hashes;
    key_hashes.reserve(from.size());
    for (const auto& kv : from)
    {
        const FK& key = kv.first;
        size_t h;
        if constexpr (std::is_same_v<FK, std::string>)
        {
            h = HashUtils::hashString(key.c_str());
        }
        else
        {
            h = HashUtils::hasher(key);
        }
        key_hashes.push_back(h);
    }
    size_t khi = 0;
    for (const auto& kv : from)
    {
        (void)kv;
        size_t bucketIndex = key_hashes[khi++] % hashMod;
        buckets[bucketIndex].beginIndex++;
    }

    size_t beginIndex = 0;
    for (size_t bucketIndex = 0; bucketIndex < numBuckets; bucketIndex++)
    {
        typename zm::HashMap<Key, Value>::Bucket& bucket = buckets[bucketIndex];
        size_t numElementsInBucket = bucket.beginIndex;
        bucket.beginIndex = uint32_t(beginIndex);
        bucket.endIndex = bucket.beginIndex;
        beginIndex += numElementsInBucket;
    }

    using ItemType = zm::Pair<const Key, Value>;
    constexpr size_t alignOfItem = std::alignment_of<ItemType>::value;
    constexpr size_t sizeOfItem = sizeof(ItemType);
    zm::goffset_t itemsDataOffset = alloc_aligned(sizeOfItem * numElements, alignOfItem);

    ItemType* items = reinterpret_cast<ItemType*>(get_ptr_unsafe_to_store(itemsDataOffset));
    buckets = reinterpret_cast<typename zm::HashMap<Key, Value>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));

    khi = 0;
    for (const auto& kv : from)
    {
        const FK& key = kv.first;
        const FV& value = kv.second;
        size_t bucketIndex = key_hashes[khi++] % hashMod;
        typename zm::HashMap<Key, Value>::Bucket& bucket = buckets[bucketIndex];

        ItemType* element = &items[bucket.endIndex];

        Key* mutableKey = const_cast<Key*>(&element->first);
        BuilderBase::placementCtor<Key>(mutableKey);
        BuilderBase::placementCtor<Value>(&element->second);

        *mutableKey = key;
        element->second = value;

        bucket.endIndex++;
    }

    to = reinterpret_cast<zm::HashMap<Key, Value>*>(get_ptr_unsafe_to_store(to_offset));
    to->buckets.numElements = uint32_t(numBuckets);
    to->buckets.relativeOffset = get_relative_offset(&to->buckets, bucketsDataOffset);
    to->items.numElements = uint32_t(numElements);
    to->items.relativeOffset = get_relative_offset(&to->items, itemsDataOffset);
    register_roffset_slot(get_global_offset(&to->buckets), bucketsDataOffset);
    register_roffset_slot(get_global_offset(&to->items), itemsDataOffset);
}

template <typename TRoot> class Builder : public BuilderBase
{
  public:
    explicit Builder(size_t initialSizeInBytes, PrivateToken tk)
        : BuilderBase(initialSizeInBytes, tk)
    {
        // Automatically allocate the root object at offset 0
        allocate<TRoot>();
    }

    using TSelf = Builder<TRoot>;
    static std::unique_ptr<TSelf> create(size_t initialSizeInBytes = kDefaultWriteBlobArenaReserveBytes)
    {
        std::unique_ptr<TSelf> res = std::make_unique<TSelf>(initialSizeInBytes, PrivateToken{});
        return res;
    }

    TRoot* getRoot() { return reinterpret_cast<TRoot*>(get_ptr_unsafe_to_store(0)); }
};

} // namespace detail

/*

**Blob writing**

User-facing entry is **`zm::write_blob`** and **`BlobWriter`** only. Implementation types live in **`zm::detail`**.

*/

namespace detail
{
template <typename TRoot, typename Fn>
std::vector<char> write_blob_with_initial_buffer_bytes(Fn&& fn, size_t initialBufferBytes, size_t finalizeAlignment = 4);
}

template <typename TRoot, typename Fn>
std::vector<char> write_blob(Fn&& fn, size_t finalizeAlignment);

template <typename TRoot>
class BlobWriter
{
    struct Private {};

    template <typename R, typename Fn>
    friend std::vector<char> write_blob(Fn&& fn, size_t finalizeAlignment);

    template <typename R, typename Fn>
    friend std::vector<char> detail::write_blob_with_initial_buffer_bytes(Fn&& fn, size_t initialBufferBytes, size_t finalizeAlignment);

    explicit BlobWriter(detail::Builder<TRoot>* impl, Private)
        : impl_(impl)
    {
    }

    detail::Builder<TRoot>* impl_;

  public:
    ZMEYA_NODISCARD TRoot* root() const noexcept { return impl_->getRoot(); }

    template <typename T>
    ZMEYA_NODISCARD T* allocate() { return impl_->template allocate<T>(); }

    ZMEYA_NODISCARD bool contains_pointer(const void* ptr) const { return impl_->contains_pointer(ptr); }

    ZMEYA_NODISCARD detail::BuilderBase* builder_base() const noexcept { return impl_; }

    void string_append(String& s, const char* suf);

    void string_clear(String& s);

    template <typename T, typename U> void array_push_back(Array<T>& a, U&& v);

    template <typename T> void array_pop_back(Array<T>& a);

    template <typename T> void array_clear(Array<T>& a);

    template <typename T> void array_erase_at(Array<T>& a, size_t index);

    template <typename T> void array_resize(Array<T>& a, size_t new_size, const T& fill);

    template <typename Key, typename F> void hashset_insert(HashSet<Key>& hs, const F& item);

    template <typename Key, typename F> void hashset_erase(HashSet<Key>& hs, const F& key);

    template <typename Key> void hashset_clear(HashSet<Key>& hs);

    template <typename K, typename V, typename FK, typename FV> void hashmap_insert(HashMap<K, V>& hm, const FK& key, const FV& val);

    template <typename K, typename V, typename FK> void hashmap_erase(HashMap<K, V>& hm, const FK& key);

    template <typename K, typename V> void hashmap_clear(HashMap<K, V>& hm);
};

/*

**write_blob**

Installs TLS, invokes **`fn(writer)`**, then finalizes. **`BlobWriter`** is the stable handle so internals can evolve without changing call sites.

*/

template <typename TRoot, typename Fn>
ZMEYA_NODISCARD inline std::vector<char> write_blob(Fn&& fn, size_t finalizeAlignment = 4)
{
    return detail::write_blob_with_initial_buffer_bytes<TRoot>(std::forward<Fn>(fn), detail::kDefaultWriteBlobArenaReserveBytes, finalizeAlignment);
}

namespace detail
{

/*

**write_blob_with_initial_buffer_bytes**

Same session as **`zm::write_blob`**, but **`initialBufferBytes`** seeds **`std::vector<char>::reserve`** for the arena.
Intended for unit tests that intentionally force **`std::vector`** reallocations; application code should use **`zm::write_blob`** and avoid caching raw pointers across growth regardless of starting capacity.

*/

template <typename TRoot, typename Fn>
ZMEYA_NODISCARD inline std::vector<char> write_blob_with_initial_buffer_bytes(Fn&& fn, size_t initialBufferBytes, size_t finalizeAlignment)
{
    std::unique_ptr<Builder<TRoot>> builder = Builder<TRoot>::create(initialBufferBytes);
    ScopedBuilder scope(builder.get());
    BlobWriter<TRoot> writer(builder.get(), typename BlobWriter<TRoot>::Private{});
    std::forward<Fn>(fn)(writer);
    Span<char> blobSpan = builder->finalize(finalizeAlignment);
    return std::vector<char>(blobSpan.data, blobSpan.data + blobSpan.size);
}

} // namespace detail

/*

**Global assignment operators for deep-copy conversion**

These operators enable seamless conversion between std::* and zm::* types.
They use the active Builder from TLS context and implement the deep-copy
adapter pattern for automatic nested conversion.

*/

// Forward declarations for deep-copy functions (specialized overloads only)
inline void deep_copy(const std::string& from, zm::goffset_t to_ofs);
inline void deep_copy(const char* from, zm::goffset_t to_ofs);
template <typename F, typename T> void deep_copy(const std::vector<F>& from, zm::goffset_t to_ofs);
template <typename F, typename Key> void deep_copy(const std::unordered_set<F>& from, zm::goffset_t to_ofs);
template <typename FK, typename FV, typename Key, typename Value> void deep_copy(const std::unordered_map<FK, FV>& from, zm::goffset_t to_ofs);
template <typename F1, typename F2, typename T1, typename T2> void deep_copy(const Pair<F1, F2>& from, zm::goffset_t to_ofs);
template <typename F, typename T> void deep_copy(detail::BuilderBase& builder, const F& from, zm::goffset_t to_ofs);

namespace detail
{

template <typename T> void assign_pointer(BuilderBase& builder, zm::Pointer<T>& _to, T* from)
{
    goffset_t to_offset = builder.get_global_offset(&_to);
    if (from == nullptr)
    {
        zm::Pointer<T>* to = reinterpret_cast<zm::Pointer<T>*>(builder.get_ptr_unsafe_to_store(to_offset));
        to->relativeOffset = 0;
        builder.register_roffset_slot(to_offset, goffset_t(0));
        return;
    }
    ZMEYA_ASSERT(builder.contains_pointer(from) && "Pointer 'from' should belong to the builder");
    goffset_t fromOffset = builder.get_global_offset(from);
    zm::Pointer<T>* to = reinterpret_cast<zm::Pointer<T>*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->relativeOffset = builder.get_relative_offset(to, fromOffset);
    builder.register_roffset_slot(to_offset, fromOffset);
}

inline void assign_string_std(BuilderBase& builder, String& _to, const std::string& from)
{
    ScopedBuilder tls(&builder);
    goffset_t to_offset = builder.get_global_offset(&_to);
    if (from.empty())
    {
        String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
        if (!to->empty())
        {
            builder.string_clear(*to);
        }
        return;
    }
    builder.string_note_dead_existing_blob(_to);
    size_t len = from.size();
    ZMEYA_ASSERT(len <= SIZE_MAX - 1);
    zm::goffset_t stringDataOffset = builder.alloc_aligned(len + 1, 1);
    char* stringData = reinterpret_cast<char*>(builder.get_ptr_unsafe_to_store(stringDataOffset));
    std::memcpy(stringData, from.data(), len);
    stringData[len] = '\0';
    String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->data.relativeOffset = builder.get_relative_offset(&to->data, stringDataOffset);
    builder.register_roffset_slot(builder.get_global_offset(&to->data), stringDataOffset);
    builder.string_blob_phys_register(stringDataOffset, len + 1);
}

inline void assign_string_cstr(BuilderBase& builder, String& _to, const char* from)
{
    ZMEYA_ASSERT(from != nullptr);
    ScopedBuilder tls(&builder);
    goffset_t to_offset = builder.get_global_offset(&_to);
    size_t len = std::strlen(from);
    if (len == 0)
    {
        String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
        if (!to->empty())
        {
            builder.string_clear(*to);
        }
        return;
    }
    builder.string_note_dead_existing_blob(_to);
    ZMEYA_ASSERT(len <= SIZE_MAX - 1);
    zm::goffset_t stringDataOffset = builder.alloc_aligned(len + 1, 1);
    char* stringData = reinterpret_cast<char*>(builder.get_ptr_unsafe_to_store(stringDataOffset));
    std::memcpy(stringData, from, len);
    stringData[len] = '\0';
    String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->data.relativeOffset = builder.get_relative_offset(&to->data, stringDataOffset);
    builder.register_roffset_slot(builder.get_global_offset(&to->data), stringDataOffset);
    builder.string_blob_phys_register(stringDataOffset, len + 1);
}

} // namespace detail

template <typename T> void assign(zm::Pointer<T>& _to, T* from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    detail::assign_pointer(*builder, _to, from);
}

template <typename T> void assign(Array<zm::Pointer<T>>& _to, const std::vector<T*>& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    builder->impl_assign_pointer_array(_to, from);
}

inline void assign(String& _to, const std::string& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    detail::assign_string_std(*builder, _to, from);
}

inline void assign(String& _to, const char* from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    detail::assign_string_cstr(*builder, _to, from);
}

template <typename T, typename F> void assign(Array<T>& _to, const std::vector<F>& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    builder->impl_assign_array_vector(_to, from);
}

// HashSet conversions - standalone implementation
template <typename Key, typename F> void assign(HashSet<Key>& _to, const std::unordered_set<F>& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    builder->impl_assign_hashset(_to, from);
}

// HashMap conversions - standalone implementation
template <typename Key, typename Value, typename FK, typename FV>
void assign(HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    builder->impl_assign_hashmap(_to, from);
}

// Deep-copy adapter implementations

// Universal deep_copy - works for all types thanks to operator= overloads
template <typename F, typename T> void deep_copy(const F& from, zm::goffset_t to_ofs)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    T* p_to = reinterpret_cast<T*>(builder->get_ptr_unsafe_to_store(to_ofs));
    detail::BuilderBase::placementCtor<T>(p_to);
    *p_to = from; // This will call the appropriate operator= automatically!
}

template <typename F, typename T> void deep_copy(detail::BuilderBase& builder, const F& from, zm::goffset_t to_ofs)
{
    T* p_to = reinterpret_cast<T*>(builder.get_ptr_unsafe_to_store(to_ofs));
    detail::BuilderBase::placementCtor<T>(p_to);
    detail::ScopedBuilder scope(&builder);
    *p_to = from;
}

template <typename T> void assign(detail::BuilderBase& builder, zm::Pointer<T>& _to, T* from)
{
    detail::assign_pointer(builder, _to, from);
}

template <typename T> void assign(detail::BuilderBase& builder, Array<zm::Pointer<T>>& _to, const std::vector<T*>& from)
{
    builder.impl_assign_pointer_array(_to, from);
}

inline void assign(detail::BuilderBase& builder, String& _to, const std::string& from)
{
    detail::assign_string_std(builder, _to, from);
}

inline void assign(detail::BuilderBase& builder, String& _to, const char* from)
{
    detail::assign_string_cstr(builder, _to, from);
}

template <typename T, typename F> void assign(detail::BuilderBase& builder, Array<T>& _to, const std::vector<F>& from)
{
    builder.impl_assign_array_vector(_to, from);
}

template <typename Key, typename F> void assign(detail::BuilderBase& builder, HashSet<Key>& _to, const std::unordered_set<F>& from)
{
    builder.impl_assign_hashset(_to, from);
}

template <typename Key, typename Value, typename FK, typename FV>
void assign(detail::BuilderBase& builder, HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from)
{
    builder.impl_assign_hashmap(_to, from);
}

template <typename Key, typename F> void hashset_insert(detail::BuilderBase& builder, HashSet<Key>& hs, const F& item)
{
    if constexpr (std::is_same_v<Key, String> && std::is_same_v<F, std::string>)
    {
        if (hs.contains(item.c_str()))
        {
            return;
        }
    }
    else if constexpr (std::is_same_v<Key, F>)
    {
        if (hs.contains(item))
        {
            return;
        }
    }
    std::unordered_set<F> tmp;
    tmp.reserve(hs.size() + 16);
    for (const Key& k : hs)
    {
        if constexpr (std::is_same<Key, String>::value && std::is_same<F, std::string>::value)
        {
            tmp.insert(std::string(k.c_str()));
        }
        else
        {
            tmp.insert(static_cast<F>(k));
        }
    }
    tmp.insert(item);
    builder.impl_assign_hashset(hs, tmp);
}

template <typename Key, typename F> void hashset_erase(detail::BuilderBase& builder, HashSet<Key>& hs, const F& key)
{
    std::unordered_set<F> tmp;
    tmp.reserve(hs.size());
    for (const Key& k : hs)
    {
        bool skip = false;
        if constexpr (std::is_same<Key, String>::value && std::is_same<F, std::string>::value)
        {
            skip = (key == std::string(k.c_str()));
        }
        else
        {
            skip = (k == key);
        }
        if (skip)
        {
            continue;
        }
        if constexpr (std::is_same<Key, String>::value && std::is_same<F, std::string>::value)
        {
            tmp.insert(std::string(k.c_str()));
        }
        else
        {
            tmp.insert(static_cast<F>(k));
        }
    }
    builder.impl_assign_hashset(hs, tmp);
}

template <typename Key> void hashset_clear(detail::BuilderBase& builder, HashSet<Key>& hs)
{
    if constexpr (std::is_same<Key, String>::value)
    {
        builder.impl_assign_hashset(hs, std::unordered_set<std::string>{});
    }
    else
    {
        builder.impl_assign_hashset(hs, std::unordered_set<Key>{});
    }
}

template <typename K, typename V, typename FK, typename FV> void hashmap_insert(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key, const FV& val)
{
    if constexpr (std::is_same_v<K, String> && std::is_same_v<FK, std::string>)
    {
        if (V* pv = hm.find(key.c_str()))
        {
            *pv = val;
            return;
        }
    }
    else if constexpr (std::is_same_v<FK, K>)
    {
        if (V* pv = hm.find(key))
        {
            *pv = val;
            return;
        }
    }
    // Snapshot the full map and re-assign each call (O(hm.size())); large batches should use bulk assign instead.
    if constexpr (std::is_same<K, String>::value)
    {
        std::unordered_map<std::string, V> tmp;
        tmp.reserve(hm.size() + 8);
        for (const auto& it : hm)
        {
            tmp[std::string(it.first.c_str())] = it.second;
        }
        tmp[key] = val;
        builder.impl_assign_hashmap(hm, tmp);
    }
    else
    {
        std::unordered_map<K, V> tmp;
        tmp.reserve(hm.size() + 8);
        for (const auto& it : hm)
        {
            tmp[it.first] = it.second;
        }
        tmp[key] = val;
        builder.impl_assign_hashmap(hm, tmp);
    }
}

template <typename K, typename V, typename FK> void hashmap_erase(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key)
{
    if constexpr (std::is_same<K, String>::value)
    {
        std::unordered_map<std::string, V> tmp;
        for (const auto& it : hm)
        {
            if (key == std::string(it.first.c_str()))
            {
                continue;
            }
            tmp.emplace(std::string(it.first.c_str()), it.second);
        }
        builder.impl_assign_hashmap(hm, tmp);
    }
    else
    {
        std::unordered_map<K, V> tmp;
        for (const auto& it : hm)
        {
            if (it.first == key)
            {
                continue;
            }
            tmp.emplace(it.first, it.second);
        }
        builder.impl_assign_hashmap(hm, tmp);
    }
}

template <typename K, typename V> void hashmap_clear(detail::BuilderBase& builder, HashMap<K, V>& hm)
{
    if constexpr (std::is_same<K, String>::value)
    {
        builder.impl_assign_hashmap(hm, std::unordered_map<std::string, V>{});
    }
    else
    {
        builder.impl_assign_hashmap(hm, std::unordered_map<K, V>{});
    }
}

template <typename T> template <typename U> inline void Array<T>::push_back(U&& v)
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_push_back(*this, std::forward<U>(v));
}

template <typename T> inline void Array<T>::pop_back()
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_pop_back(*this);
}

template <typename T> inline void Array<T>::clear()
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_clear(*this);
}

template <typename T> inline void Array<T>::erase_at(size_t index)
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_erase_at(*this, index);
}

template <typename T> inline void Array<T>::resize(size_t new_size, const T& fill)
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_resize_fill(*this, new_size, fill);
}

inline void String::clear()
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->string_clear(*this);
}

inline String& String::append(const char* suf)
{
    ZMEYA_ASSERT(suf != nullptr);
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->string_append_cstr(*this, suf, std::strlen(suf));
    return *this;
}

inline String& String::operator+=(const char* suf) { return append(suf); }

template <typename Key> template <typename F> inline void HashSet<Key>::insert(const F& item) { zm::hashset_insert(*detail::get_global_builder(), *this, item); }

template <typename Key> template <typename F> inline void HashSet<Key>::erase(const F& key) { zm::hashset_erase(*detail::get_global_builder(), *this, key); }

template <typename Key> inline void HashSet<Key>::clear() { zm::hashset_clear(*detail::get_global_builder(), *this); }

template <typename K, typename V> template <typename FK, typename FV> inline void HashMap<K, V>::insert(const FK& key, const FV& val)
{
    zm::hashmap_insert(*detail::get_global_builder(), *this, key, val);
}

template <typename K, typename V> template <typename FK> inline void HashMap<K, V>::erase(const FK& key) { zm::hashmap_erase(*detail::get_global_builder(), *this, key); }

template <typename K, typename V> inline void HashMap<K, V>::clear() { zm::hashmap_clear(*detail::get_global_builder(), *this); }

template <typename TRoot> inline void BlobWriter<TRoot>::string_append(String& s, const char* suf)
{
    ZMEYA_ASSERT(suf != nullptr);
    impl_->string_append_cstr(s, suf, std::strlen(suf));
}

template <typename TRoot> inline void BlobWriter<TRoot>::string_clear(String& s) { impl_->string_clear(s); }

template <typename TRoot> template <typename T, typename U> inline void BlobWriter<TRoot>::array_push_back(Array<T>& a, U&& v) { impl_->array_push_back(a, std::forward<U>(v)); }

template <typename TRoot> template <typename T> inline void BlobWriter<TRoot>::array_pop_back(Array<T>& a) { impl_->array_pop_back(a); }

template <typename TRoot> template <typename T> inline void BlobWriter<TRoot>::array_clear(Array<T>& a) { impl_->array_clear(a); }

template <typename TRoot> template <typename T> inline void BlobWriter<TRoot>::array_erase_at(Array<T>& a, size_t index) { impl_->array_erase_at(a, index); }

template <typename TRoot> template <typename T> inline void BlobWriter<TRoot>::array_resize(Array<T>& a, size_t new_size, const T& fill) { impl_->array_resize_fill(a, new_size, fill); }

template <typename TRoot> template <typename Key, typename F> inline void BlobWriter<TRoot>::hashset_insert(HashSet<Key>& hs, const F& item) { zm::hashset_insert(*impl_, hs, item); }

template <typename TRoot> template <typename Key, typename F> inline void BlobWriter<TRoot>::hashset_erase(HashSet<Key>& hs, const F& key) { zm::hashset_erase(*impl_, hs, key); }

template <typename TRoot> template <typename Key> inline void BlobWriter<TRoot>::hashset_clear(HashSet<Key>& hs) { zm::hashset_clear(*impl_, hs); }

template <typename TRoot> template <typename K, typename V, typename FK, typename FV>
inline void BlobWriter<TRoot>::hashmap_insert(HashMap<K, V>& hm, const FK& key, const FV& val)
{
    zm::hashmap_insert(*impl_, hm, key, val);
}

template <typename TRoot> template <typename K, typename V, typename FK> inline void BlobWriter<TRoot>::hashmap_erase(HashMap<K, V>& hm, const FK& key)
{
    zm::hashmap_erase(*impl_, hm, key);
}

template <typename TRoot> template <typename K, typename V> inline void BlobWriter<TRoot>::hashmap_clear(HashMap<K, V>& hm) { zm::hashmap_clear(*impl_, hm); }

template <typename Key, typename F> void hashset_insert(detail::BuilderBase& builder, HashSet<Key>& hs, const F& item);

template <typename Key, typename F> void hashset_erase(detail::BuilderBase& builder, HashSet<Key>& hs, const F& key);

template <typename Key> void hashset_clear(detail::BuilderBase& builder, HashSet<Key>& hs);

template <typename K, typename V, typename FK, typename FV> void hashmap_insert(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key, const FV& val);

template <typename K, typename V, typename FK> void hashmap_erase(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key);

template <typename K, typename V> void hashmap_clear(detail::BuilderBase& builder, HashMap<K, V>& hm);

/*

**Helper syntax for clean assignments**

Container and **`zm::Pointer`** types expose **`operator=`** from STL-shaped values (and raw pointers for **`Pointer`**). Prefer normal assignments inside **`zm::write_blob`**.

Usage examples:
  root->string_array = src_vector;

*/

#define ZM_ASSIGN(zm_var, std_var) ((zm_var) = (std_var))

#endif // ZMEYA_ENABLE_SERIALIZE_SUPPORT

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace zm

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
