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

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
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

    goffset_t alloc_aligned(size_t numBytes, size_t alignment)
    {
        ZMEYA_ASSERT(isPowerOfTwo(alignment));
        ZMEYA_ASSERT(alignment <= ZMEYA_MAX_ALIGN);

        size_t cursor = data.size();

        // Calculate padding for alignment
        size_t off = cursor & (alignment - 1);
        size_t padding = (off != 0) ? (alignment - off) : 0;
        size_t allocOffset = cursor + padding;
        size_t totalBytes = numBytes + padding;

        // Resize with zero-initialization
        data.resize(data.size() + totalBytes, char(0));

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

    void* get_ptr_unsafe_to_store(goffset_t g_offs) { return &data[g_offs]; }

    template <typename T> T* allocate()
    {
        static_assert(std::is_trivially_copyable<T>::value, "Only trivially copyable types allowed");
        goffset_t g_offs = alloc_aligned(sizeof(T), alignof(T));
        void* ptr = get_ptr_unsafe_to_store(g_offs);
        BuilderBase::placementCtor<T>(ptr);
        return reinterpret_cast<T*>(ptr);
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

    Span<char> finalize(size_t alignment = 4)
    {
        // Add padding to ensure final size is multiple of alignment
        size_t currentSize = data.size();
        size_t remainder = currentSize % alignment;
        if (remainder != 0)
        {
            size_t paddingNeeded = alignment - remainder;
            data.resize(currentSize + paddingNeeded, char(0));
        }

        ZMEYA_ASSERT((data.size() % alignment) == 0);
        return Span<char>(data.data(), data.size());
    }

    template <typename T> roffset_t get_relative_offset(const T* base, goffset_t ofs) const
    {
        ZMEYA_ASSERT(is_stack_pointer(base) == false && "Stack pointer detected!");
        ZMEYA_ASSERT(contains_pointer(base) && "A pointer should belong to the builder");

        goffset_t baseOffset = get_global_offset(base);
        diff_t diff = ofs - baseOffset;

        ZMEYA_ASSERT(diff >= diff_t(std::numeric_limits<roffset_t>::min()));
        ZMEYA_ASSERT(diff <= diff_t(std::numeric_limits<roffset_t>::max()));
        return roffset_t(diff);
    }


};

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
    static std::unique_ptr<TSelf> create(size_t initialSizeInBytes = 2048)
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

template <typename TRoot, typename Fn>
std::vector<char> write_blob(Fn&& fn, size_t initialSizeInBytes, size_t finalizeAlignment);

template <typename TRoot>
class BlobWriter
{
    struct Private {};

    template <typename R, typename Fn>
    friend std::vector<char> write_blob(Fn&& fn, size_t initialSizeInBytes, size_t finalizeAlignment);

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
};

/*

**write_blob**

Installs TLS, invokes **`fn(writer)`**, then finalizes. **`BlobWriter`** is the stable handle so internals can evolve without changing call sites.

*/

template <typename TRoot, typename Fn>
ZMEYA_NODISCARD inline std::vector<char> write_blob(Fn&& fn, size_t initialSizeInBytes = 2048, size_t finalizeAlignment = 4)
{
    std::unique_ptr<detail::Builder<TRoot>> builder = detail::Builder<TRoot>::create(initialSizeInBytes);
    detail::ScopedBuilder scope(builder.get());
    BlobWriter<TRoot> writer(builder.get(), typename BlobWriter<TRoot>::Private{});
    std::forward<Fn>(fn)(writer);
    Span<char> blobSpan = builder->finalize(finalizeAlignment);
    return std::vector<char>(blobSpan.data, blobSpan.data + blobSpan.size);
}

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

// Pointer assignment function
template <typename T> void assign(zm::Pointer<T>& _to, T* from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    goffset_t to_offset = builder->get_global_offset(&_to);

    if (from == nullptr)
    {
        zm::Pointer<T>* to = reinterpret_cast<zm::Pointer<T>*>(builder->get_ptr_unsafe_to_store(to_offset));
        to->relativeOffset = 0; // null pointer
        return;
    }

    // Get offsets and calculate relative offset safely
    ZMEYA_ASSERT(builder->contains_pointer(from) && "Pointer 'from' should belong to the builder");
    
    goffset_t fromOffset = builder->get_global_offset(from);
    
    zm::Pointer<T>* to = reinterpret_cast<zm::Pointer<T>*>(builder->get_ptr_unsafe_to_store(to_offset));
    to->relativeOffset = builder->get_relative_offset(to, fromOffset);
}

// Array of pointers assignment function
template <typename T> void assign(Array<zm::Pointer<T>>& _to, const std::vector<T*>& from)
{
    if (from.empty())
    {
        return; // Array is already default-initialized as empty
    }

    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    goffset_t to_offset = builder->get_global_offset(&_to);

    // Check if array is already assigned
    Array<zm::Pointer<T>>* to = reinterpret_cast<Array<zm::Pointer<T>>*>(builder->get_ptr_unsafe_to_store(to_offset));
    if (to->numElements != 0 || to->relativeOffset != 0)
    {
        ZMEYA_ASSERT(false && "Array already assigned - multiple assignments not supported to avoid builder's memory fragmentation");
        return;
    }

    // Allocate array data
    constexpr size_t alignOfPtr = std::alignment_of<zm::Pointer<T>>::value;
    constexpr size_t sizeOfPtr = sizeof(zm::Pointer<T>);

    zm::goffset_t arrayDataOffset = builder->alloc_aligned(sizeOfPtr * from.size(), alignOfPtr);

    // Update array metadata
    to = reinterpret_cast<Array<zm::Pointer<T>>*>(builder->get_ptr_unsafe_to_store(to_offset));
    to->numElements = uint32_t(from.size());
    to->relativeOffset = builder->get_relative_offset(to, arrayDataOffset);

    // Initialize and fill array elements
    for (size_t i = 0; i < from.size(); ++i)
    {
        // Calculate offset for this element
        zm::goffset_t elementOffset = zm::goffset_t(arrayDataOffset + i * sizeOfPtr);
        
        // Get pointer to element and initialize it
        zm::Pointer<T>* element = reinterpret_cast<zm::Pointer<T>*>(builder->get_ptr_unsafe_to_store(elementOffset));
        detail::BuilderBase::placementCtor<zm::Pointer<T>>(element);
        
        // Assign the pointer value
        assign(*element, from[i]);
    }
}

// String conversions - standalone implementation
inline void assign(String& _to, const std::string& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    goffset_t to_offset = builder->get_global_offset(&_to);

    if (from.empty())
    {
        // Leave as default-initialized (empty string)
        return;
    }

    size_t len = from.size();
    zm::goffset_t stringDataOffset = builder->alloc_aligned(len + 1, 1);

    // Copy string data without storing pointer
    char* stringData = (char*)builder->get_ptr_unsafe_to_store(stringDataOffset);
    std::memcpy(stringData, from.data(), len);
    stringData[len] = '\0';

    String* to = reinterpret_cast<String*>(builder->get_ptr_unsafe_to_store(to_offset));
    to->data.relativeOffset = builder->get_relative_offset(&to->data, stringDataOffset);
}

inline void assign(String& _to, const char* from)
{
    ZMEYA_ASSERT(from != nullptr);

    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    goffset_t to_offset = builder->get_global_offset(&_to);

    size_t len = std::strlen(from);
    if (len == 0)
    {
        // Leave as default-initialized (empty string)
        return;
    }

    zm::goffset_t stringDataOffset = builder->alloc_aligned(len + 1, 1);

    // Copy string data without storing pointer
    char* stringData = (char*)builder->get_ptr_unsafe_to_store(stringDataOffset);
    std::memcpy(stringData, from, len);
    stringData[len] = '\0';

    String* to = reinterpret_cast<String*>(builder->get_ptr_unsafe_to_store(to_offset));
    to->data.relativeOffset = builder->get_relative_offset(&to->data, stringDataOffset);
}

// Array conversions - standalone implementation
template <typename T, typename F> void assign(Array<T>& _to, const std::vector<F>& from)
{
    // Allow empty arrays
    if (from.empty())
    {
        return; // Array is already default-initialized as empty
    }

    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    goffset_t to_offset = builder->get_global_offset(&_to);

    // Check if array is already assigned
    Array<T>* to = reinterpret_cast<Array<T>*>(builder->get_ptr_unsafe_to_store(to_offset));
    if (to->numElements != 0 || to->relativeOffset != 0)
    {
        ZMEYA_ASSERT(false && "Array already assigned - multiple assignments not supported to avoid builder's memory fragmentation");
        return;
    }

    // Allocate array data
    constexpr size_t alignOfT = std::alignment_of<T>::value;
    constexpr size_t sizeOfT = sizeof(T);

    zm::goffset_t arrayDataOffset = builder->alloc_aligned(sizeOfT * from.size(), alignOfT);

    // Update array metadata
    to = reinterpret_cast<Array<T>*>(builder->get_ptr_unsafe_to_store(to_offset));
    to->numElements = uint32_t(from.size());
    to->relativeOffset = builder->get_relative_offset(to, arrayDataOffset);

    for (size_t i = 0; i < from.size(); ++i)
    {
        // deep-copy elements using the universal deep_copy function
        zm::goffset_t elementOffset = zm::goffset_t(arrayDataOffset + i * sizeOfT);
        deep_copy<F, T>(from[i], elementOffset);
    }
}

// HashSet conversions - standalone implementation
template <typename Key, typename F> void assign(HashSet<Key>& _to, const std::unordered_set<F>& from)
{
    if (from.empty())
    {
        return; // HashSet is already default-initialized as empty
    }

    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    goffset_t to_offset = builder->get_global_offset(&_to);

    size_t numElements = from.size();
    size_t numBuckets = numElements * 2;
    ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
    size_t hashMod = numBuckets;

    // Allocate buckets array
    constexpr size_t alignOfBucket = std::alignment_of<typename HashSet<Key>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename HashSet<Key>::Bucket);
    zm::goffset_t bucketsDataOffset = builder->alloc_aligned(sizeOfBucket * numBuckets, alignOfBucket);

    // Initialize buckets to zero
    typename HashSet<Key>::Bucket* buckets = reinterpret_cast<typename HashSet<Key>::Bucket*>(builder->get_ptr_unsafe_to_store(bucketsDataOffset));
    for (size_t i = 0; i < numBuckets; ++i) {
        detail::BuilderBase::placementCtor<typename HashSet<Key>::Bucket>(&buckets[i], HashSet<Key>::Bucket{0, 0});
    }

    // First pass: count elements per bucket
    for (const auto& item : from) {
        size_t hash;
        if constexpr (std::is_same_v<F, std::string>) {
            hash = HashUtils::hashString(item.c_str());
        } else {
            hash = HashUtils::hasher(item);
        }
        size_t bucketIndex = hash % hashMod;
        buckets[bucketIndex].beginIndex++; // temporarily use beginIndex to count
    }

    // Convert counts to ranges
    size_t beginIndex = 0;
    for (size_t bucketIndex = 0; bucketIndex < numBuckets; bucketIndex++) {
        typename HashSet<Key>::Bucket& bucket = buckets[bucketIndex];
        size_t numElementsInBucket = bucket.beginIndex;
        bucket.beginIndex = uint32_t(beginIndex);
        bucket.endIndex = bucket.beginIndex;
        beginIndex += numElementsInBucket;
    }

    // Allocate items array
    constexpr size_t alignOfKey = std::alignment_of<Key>::value;
    constexpr size_t sizeOfKey = sizeof(Key);
    zm::goffset_t itemsDataOffset = builder->alloc_aligned(sizeOfKey * numElements, alignOfKey);

    // Second pass: copy items to their buckets
    Key* items = reinterpret_cast<Key*>(builder->get_ptr_unsafe_to_store(itemsDataOffset));
    buckets = reinterpret_cast<typename HashSet<Key>::Bucket*>(builder->get_ptr_unsafe_to_store(bucketsDataOffset)); // refresh pointer

    for (const auto& item : from) {
        size_t hash;
        if constexpr (std::is_same_v<F, std::string>) {
            hash = HashUtils::hashString(item.c_str());
        } else {
            hash = HashUtils::hasher(item);
        }
        size_t bucketIndex = hash % hashMod;
        typename HashSet<Key>::Bucket& bucket = buckets[bucketIndex];
        
        // Place item at current endIndex and increment
        Key* element = &items[bucket.endIndex];
        detail::BuilderBase::placementCtor<Key>(element);
        
        // Assign the item using operator= (automatic conversion)
        *element = item;
        
        bucket.endIndex++;
    }

    // Update HashSet metadata
    HashSet<Key>* to = reinterpret_cast<HashSet<Key>*>(builder->get_ptr_unsafe_to_store(to_offset));
    to->buckets.numElements = uint32_t(numBuckets);
    to->buckets.relativeOffset = builder->get_relative_offset(&to->buckets, bucketsDataOffset);
    to->items.numElements = uint32_t(numElements);
    to->items.relativeOffset = builder->get_relative_offset(&to->items, itemsDataOffset);
}

// HashMap conversions - standalone implementation
template <typename Key, typename Value, typename FK, typename FV>
void assign(HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from)
{
    if (from.empty())
    {
        return; // HashMap is already default-initialized as empty
    }

    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    goffset_t to_offset = builder->get_global_offset(&_to);

    size_t numElements = from.size();
    size_t numBuckets = numElements * 2;
    ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
    size_t hashMod = numBuckets;

    // Allocate buckets array
    constexpr size_t alignOfBucket = std::alignment_of<typename HashMap<Key, Value>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename HashMap<Key, Value>::Bucket);
    zm::goffset_t bucketsDataOffset = builder->alloc_aligned(sizeOfBucket * numBuckets, alignOfBucket);

    // Initialize buckets to zero
    typename HashMap<Key, Value>::Bucket* buckets = reinterpret_cast<typename HashMap<Key, Value>::Bucket*>(builder->get_ptr_unsafe_to_store(bucketsDataOffset));
    for (size_t i = 0; i < numBuckets; ++i) {
        detail::BuilderBase::placementCtor<typename HashMap<Key, Value>::Bucket>(&buckets[i], HashMap<Key, Value>::Bucket{0, 0});
    }

    // First pass: count elements per bucket
    for (const auto& [key, value] : from) {
        size_t hash;
        if constexpr (std::is_same_v<FK, std::string>) {
            hash = HashUtils::hashString(key.c_str());
        } else {
            hash = HashUtils::hasher(key);
        }
        size_t bucketIndex = hash % hashMod;
        buckets[bucketIndex].beginIndex++; // temporarily use beginIndex to count
    }

    // Convert counts to ranges
    size_t beginIndex = 0;
    for (size_t bucketIndex = 0; bucketIndex < numBuckets; bucketIndex++) {
        typename HashMap<Key, Value>::Bucket& bucket = buckets[bucketIndex];
        size_t numElementsInBucket = bucket.beginIndex;
        bucket.beginIndex = uint32_t(beginIndex);
        bucket.endIndex = bucket.beginIndex;
        beginIndex += numElementsInBucket;
    }

    // Allocate items array (Pair<Key, Value>)
    using ItemType = Pair<const Key, Value>;
    constexpr size_t alignOfItem = std::alignment_of<ItemType>::value;
    constexpr size_t sizeOfItem = sizeof(ItemType);
    zm::goffset_t itemsDataOffset = builder->alloc_aligned(sizeOfItem * numElements, alignOfItem);

    // Second pass: copy items to their buckets
    ItemType* items = reinterpret_cast<ItemType*>(builder->get_ptr_unsafe_to_store(itemsDataOffset));
    buckets = reinterpret_cast<typename HashMap<Key, Value>::Bucket*>(builder->get_ptr_unsafe_to_store(bucketsDataOffset)); // refresh pointer

    for (const auto& [key, value] : from) {
        size_t hash;
        if constexpr (std::is_same_v<FK, std::string>) {
            hash = HashUtils::hashString(key.c_str());
        } else {
            hash = HashUtils::hasher(key);
        }
        size_t bucketIndex = hash % hashMod;
        typename HashMap<Key, Value>::Bucket& bucket = buckets[bucketIndex];
        
        // Place item at current endIndex and increment
        ItemType* element = &items[bucket.endIndex];

        // Assign key and value using the same logic as Array assign
        // Note: element->first is const Key, so we need to cast away const for assignment
        Key* mutableKey = const_cast<Key*>(&element->first);
        detail::BuilderBase::placementCtor<Key>(mutableKey);
        detail::BuilderBase::placementCtor<Value>(&element->second);
        
        // Assign key and value using operator= (automatic conversion)
        *mutableKey = key;
        element->second = value;
        
        bucket.endIndex++;
    }

    // Update HashMap metadata
    HashMap<Key, Value>* to = reinterpret_cast<HashMap<Key, Value>*>(builder->get_ptr_unsafe_to_store(to_offset));
    to->buckets.numElements = uint32_t(numBuckets);
    to->buckets.relativeOffset = builder->get_relative_offset(&to->buckets, bucketsDataOffset);
    to->items.numElements = uint32_t(numElements);
    to->items.relativeOffset = builder->get_relative_offset(&to->items, itemsDataOffset);
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
