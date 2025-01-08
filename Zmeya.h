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
#define ZMEYA_ASSERT(cond) assert(cond)
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
template <typename T> class BlobPtr;
#endif

/*
    Pointer - self-relative pointer relative to its own memory address
*/
template <typename T> class Pointer
{
    // addr = this + offset
    // offset(0) = this = nullptr (here is the limitation, pointer can't point to itself)
    // this extra offset fits well into the x86/ARM addressing modes
    // see for details  https://godbolt.org/z/aTTW9E7o9
    roffset_t relativeOffset;

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

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    Pointer& operator=(const BlobPtr<T>& other);
#endif

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

    friend class BlobBuilder;
};

/*
    String
*/
class String
{
    Pointer<char> data;

  public:
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

    ZMEYA_NODISCARD bool empty() const noexcept { return data.get() != nullptr; }
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

    friend class BlobBuilder;
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

    friend class BlobBuilder;
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
    friend class BlobBuilder;
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

    friend class BlobBuilder;
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

class BlobBuilder;

/*
    This is a non-serializable pointer to blob internal memory
    Note: blob is able to relocate its own memory that's is why we cannot use
   standard pointers or references
*/
template <typename T> class BlobPtr
{
    std::weak_ptr<const BlobBuilder> blob;
    offset_t absoluteOffset = 0;

    bool isEqual(const BlobPtr<T>& other) const
    {
        if (blob.lock() != other.blob.lock())
        {
            return false;
        }
        return absoluteOffset == other.absoluteOffset;
    }

  public:
    explicit BlobPtr(std::weak_ptr<const BlobBuilder>&& _blob, offset_t _absoluteOffset)
        : blob(std::move(_blob))
        , absoluteOffset(_absoluteOffset)
    {
    }

    BlobPtr() = default;

    BlobPtr(BlobPtr&&) = default;
    BlobPtr& operator=(BlobPtr&&) = default;
    BlobPtr(const BlobPtr&) = default;
    BlobPtr& operator=(const BlobPtr&) = default;

    template <typename T2> BlobPtr(const BlobPtr<T2>& other)
    {
        static_assert(std::is_convertible<T2*, T*>::value, "Uncompatible types");
        blob = other.blob;
        absoluteOffset = other.absoluteOffset;
    }
    template <class T2> friend class BlobPtr;

    offset_t getAbsoluteOffset() const { return absoluteOffset; }

    ZMEYA_NODISCARD T* get() const;

    T* operator->() const { return get(); }
    T& operator*() const { return *(get()); }

    operator bool() const { return get() != nullptr; }
    bool operator==(const BlobPtr& other) const { return isEqual(other); }
    bool operator!=(const BlobPtr& other) const { return !isEqual(other); }

    template <typename T2> friend class Pointer;
};

/*
    Blob allocator - aligned allocator for internal Blob usage
*/
template <typename T, int Alignment> class BlobBuilderAllocator : public std::allocator<T>
{
  public:
    typedef size_t size_type;
    typedef T* pointer;
    typedef const T* const_pointer;

    template <typename _Tp1> struct rebind
    {
        typedef BlobBuilderAllocator<_Tp1, Alignment> other;
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

    BlobBuilderAllocator()
        : std::allocator<T>()
    {
    }
    BlobBuilderAllocator(const BlobBuilderAllocator& a)
        : std::allocator<T>(a)
    {
    }
    template <class U>
    BlobBuilderAllocator(const BlobBuilderAllocator<U, Alignment>& a)
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

template <typename T> std::weak_ptr<T> weak_from(T* p)
{
    std::shared_ptr<T> shared = p->shared_from_this();
    return shared;
}

/*
    Blob - a binary blob of data that is able to store POD types and special
   "movable" data structures Note: Zmeya containers can be freely moved in
   memory and deserialize from raw bytes without any extra work.
*/
class BlobBuilder : public std::enable_shared_from_this<BlobBuilder>
{
    std::vector<char, BlobBuilderAllocator<char, ZMEYA_MAX_ALIGN>> data;

  private:
    ZMEYA_NODISCARD const char* get(offset_t absoluteOffset) const
    {
        ZMEYA_ASSERT(absoluteOffset < data.size());
        return &data[absoluteOffset];
    }

    template <typename T> ZMEYA_NODISCARD BlobPtr<T> getBlobPtr(const T* p) const
    {
        ZMEYA_ASSERT(containsPointer(p));
        offset_t absoluteOffset = diffAddr(uintptr_t(p), uintptr_t(data.data()));
        return BlobPtr<T>(weak_from(this), absoluteOffset);
    }

    struct PrivateToken
    {
    };

  public:
    BlobBuilder() = delete;

    BlobBuilder(size_t initialSizeInBytes, PrivateToken)
    {
        static_assert(std::is_trivially_copyable<Pointer<int>>::value, "Pointer is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<Array<int>>::value, "Array is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<HashSet<int>>::value, "HashSet is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<Pair<int, float>>::value, "Pair is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<HashMap<int, int>>::value, "HashMap is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<String>::value, "String is_trivially_copyable check failed");

        data.reserve(initialSizeInBytes);
    }

    ~BlobBuilder() = default;

    bool containsPointer(const void* p) const { return (!data.empty() && (p >= &data.front() && p <= &data.back())); }

    BlobPtr<char> allocate(size_t numBytes, size_t alignment)
    {
        ZMEYA_ASSERT(isPowerOfTwo(alignment));
        ZMEYA_ASSERT(alignment < ZMEYA_MAX_ALIGN);
        //
        size_t cursor = data.size();

        // padding / alignment
        size_t off = cursor & (alignment - 1);
        size_t padding = 0;
        if (off != 0)
        {
            padding = alignment - off;
        }
        size_t absoluteOffset = cursor + padding;
        size_t numBytesToAllocate = numBytes + padding;

        // Allocate more memory
        // Note: new memory is filled with zeroes
        // Zmeya containers rely on this behavior and we want to have all the padding zeroed as well
        data.resize(data.size() + numBytesToAllocate, char(0));

        // check alignment
        ZMEYA_ASSERT((uintptr_t(&data[absoluteOffset]) & (alignment - 1)) == 0);
        ZMEYA_ASSERT(absoluteOffset < size_t(std::numeric_limits<offset_t>::max()));
        return BlobPtr<char>(weak_from(this), offset_t(absoluteOffset));
    }

    template <typename T, typename... _Valty> void placementCtor(void* ptr, _Valty&&... _Val)
    {
        ::new (const_cast<void*>(static_cast<const volatile void*>(ptr))) T(std::forward<_Valty>(_Val)...);
    }

    template <typename T, typename... _Valty> BlobPtr<T> allocate(_Valty&&... _Val)
    {
        // compile time checks
        static_assert(std::is_trivially_copyable<T>::value, "Only trivially copyable types allowed");
        constexpr size_t alignOfT = std::alignment_of<T>::value;
        static_assert(isPowerOfTwo(alignOfT), "Non power of two alignment not supported");
        static_assert(alignOfT < ZMEYA_MAX_ALIGN, "Unsupported alignment");
        constexpr size_t sizeOfT = sizeof(T);

        BlobPtr<char> ptr = allocate(sizeOfT, alignOfT);

        placementCtor<T>(ptr.get(), std::forward<_Valty>(_Val)...);

        return BlobPtr<T>(weak_from(this), ptr.getAbsoluteOffset());
    }

    template <typename T> T* getDirectMemoryAccessUnsafe(offset_t absoluteOffset)
    {
        const char* p = get(absoluteOffset);
        return const_cast<T*>(reinterpret_cast<const T*>(p));
    }

    template <typename T> void setArrayOffset(const BlobPtr<Array<T>>& dst, offset_t absoluteOffset)
    {
        dst->relativeOffset = toRelativeOffset(diff(absoluteOffset, dst.getAbsoluteOffset()));
    }

    template <typename T> offset_t resizeArrayWithoutInitialization(Array<T>& _dst, size_t numElements)
    {
        constexpr size_t alignOfT = std::alignment_of<T>::value;
        constexpr size_t sizeOfT = sizeof(T);
        static_assert((sizeOfT % alignOfT) == 0, "The size must be a multiple of the alignment");

        BlobPtr<Array<T>> dst = getBlobPtr(&_dst);
        // An array can be assigned/resized only once (non empty array detected)
        ZMEYA_ASSERT(dst->relativeOffset == 0 && dst->numElements == 0);

        BlobPtr<char> arrData = allocate(sizeOfT * numElements, alignOfT);
        ZMEYA_ASSERT(numElements < size_t(std::numeric_limits<uint32_t>::max()));
        dst->numElements = uint32_t(numElements);
        setArrayOffset(dst, arrData.getAbsoluteOffset());
        return arrData.getAbsoluteOffset();
    }

    // get writeable pointer to array element
    template <typename T> ZMEYA_NODISCARD BlobPtr<T> getArrayElement(Array<T>& arr, const size_t index) const noexcept
    {
        T* rawElementPtr = arr.getData() + index;
        BlobPtr<T> element = getBlobPtr(rawElementPtr);
        return element;
    }

    // resize array (using copy constructor)
    template <typename T> offset_t resizeArray(Array<T>& _dst, size_t numElements, const T& emptyElement)
    {
        BlobPtr<Array<T>> dst = getBlobPtr(&_dst);
        offset_t absoluteOffset = resizeArrayWithoutInitialization(_dst, numElements);
        T* current = getDirectMemoryAccessUnsafe<T>(absoluteOffset);
        for (size_t i = 0; i < numElements; i++)
        {
            // call copy ctor
            placementCtor<T>(current, emptyElement);
            current++;
        }
        return absoluteOffset;
    }

    // resize array (using default constructor)
    template <typename T> offset_t resizeArray(Array<T>& _dst, size_t numElements)
    {
        BlobPtr<Array<T>> dst = getBlobPtr(&_dst);
        offset_t absoluteOffset = resizeArrayWithoutInitialization(_dst, numElements);
        T* current = getDirectMemoryAccessUnsafe<T>(absoluteOffset);
        for (size_t i = 0; i < numElements; i++)
        {
            // default ctor
            placementCtor<T>(current);
            current++;
        }
        return absoluteOffset;
    }

    // copyTo array fast (without using convertor)
    template <typename T> offset_t copyToArrayFast(BlobPtr<Array<T>> dst, const T* begin, size_t numElements)
    {
        static_assert(std::is_trivially_copyable<T>::value, "Only trivially copyable types allowed");
        offset_t absoluteOffset = resizeArrayWithoutInitialization(*dst, numElements);
        T* arrData = getDirectMemoryAccessUnsafe<T>(absoluteOffset);
        std::memcpy(arrData, begin, sizeof(T) * numElements);
        return absoluteOffset;
    }

    // copyTo array fast (without using convertor)
    template <typename T> offset_t copyToArrayFast(Array<T>& _dst, const T* begin, size_t numElements)
    {
        BlobPtr<Array<T>> dst = getBlobPtr(&_dst);
        return copyToArrayFast(dst, begin, numElements);
    }

    // copyTo array from range
    template <typename T, typename Iter, typename ConvertorFunc>
    offset_t copyToArray(BlobPtr<Array<T>> dst, const Iter begin, const Iter end, int64_t size, ConvertorFunc convertorFunc)
    {
        size_t numElements = (size >= 0) ? size_t(size) : std::distance(begin, end);
        resizeArray(*dst, numElements);

        BlobPtr<T> firstElement = getBlobPtr(dst->data());
        offset_t absoluteOffset = firstElement.getAbsoluteOffset();
        offset_t currentIndex = 0;
        for (Iter cur = begin; cur != end; ++cur)
        {
            offset_t currentItemAbsoluteOffset = absoluteOffset + sizeof(T) * currentIndex;
            convertorFunc(this, currentItemAbsoluteOffset, *cur);
            currentIndex++;
        }
        return absoluteOffset;
    }

    // copyTo array from range
    template <typename T, typename Iter, typename ConvertorFunc>
    offset_t copyToArray(Array<T>& _dst, const Iter begin, const Iter end, int64_t size, ConvertorFunc convertorFunc)
    {
        BlobPtr<Array<T>> dst = getBlobPtr(&_dst);
        return copyToArray(dst, begin, end, size, convertorFunc);
    }

    // copyTo hash container
    template <typename ItemSrcAdapter, typename ItemDstAdapter, typename HashType, typename Iter, typename ConvertorFunc>
    void copyToHash(HashType& _dst, Iter begin, Iter end, int64_t size, ConvertorFunc convertorFunc)
    {
        // Note: this bucketing method relies on the fact that the input data set is (already) unique
        size_t numElements = (size >= 0) ? size_t(size) : std::distance(begin, end);
        ZMEYA_ASSERT(numElements > 0);
        size_t numBuckets = numElements * 2;
        ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
        size_t hashMod = numBuckets;

        BlobPtr<HashType> dst = getBlobPtr(&_dst);
        // allocate buckets & items
        resizeArray(dst->buckets, numBuckets);

        // 1-st pass count the number of elements per bucket (beginIndex / endIndex)
        typename HashType::Bucket* buckets = dst->buckets.get_raw_ptr_unsafe_can_be_relocated();
        for (Iter cur = begin; cur != end; ++cur)
        {
            const auto& current = *cur;
            size_t hash = ItemSrcAdapter::hash(current);
            size_t bucketIndex = hash % hashMod;
            buckets[bucketIndex].beginIndex++; // temporary use beginIndex to store the number of items
        }

        size_t beginIndex = 0;
        for (size_t bucketIndex = 0; bucketIndex < numBuckets; bucketIndex++)
        {
            typename HashType::Bucket& bucket = buckets[bucketIndex];
            size_t numElementsInBucket = bucket.beginIndex;
            bucket.beginIndex = uint32_t(beginIndex);
            bucket.endIndex = bucket.beginIndex;
            beginIndex += numElementsInBucket;
        }

        // note: at this point this pointer is no longer valid (resize array can move data)
        buckets = nullptr;

        // 2-st pass copy items
        offset_t absoluteOffset = resizeArrayWithoutInitialization(dst->items, numElements);
        for (Iter cur = begin; cur != end; ++cur)
        {
            const auto& current = *cur;
            size_t hash = ItemSrcAdapter::hash(current);
            size_t bucketIndex = hash % hashMod;
            typename HashType::Bucket* bucket = dst->buckets.get_element_ptr_unsafe_can_be_relocated(bucketIndex);
            uint32_t elementIndex = bucket->endIndex;
            offset_t currentItemAbsoluteOffset = absoluteOffset + sizeof(typename ItemDstAdapter::ItemType) * offset_t(elementIndex);
            convertorFunc(this, currentItemAbsoluteOffset, *cur);

            // Note: convertorFunc can allocate additional memory -> reallocate storage
            //    Hence, we need to reacquire the pointer
            bucket = dst->buckets.get_element_ptr_unsafe_can_be_relocated(bucketIndex);
#ifdef ZMEYA_VALIDATE_HASH_DUPLICATES
            const typename ItemDstAdapter::ItemType* lastItem =
                getDirectMemoryAccessUnsafe<typename ItemDstAdapter::ItemType>(currentItemAbsoluteOffset);
            size_t newItemHash = ItemDstAdapter::hash(*lastItem);
            // inconsistent hashing! hash(srcItem) != hash(dstItem)
            ZMEYA_ASSERT(hash == newItemHash);
            for (uint32_t testElementIndex = bucket->beginIndex; testElementIndex < bucket->endIndex; testElementIndex++)
            {
                offset_t testItemAbsoluteOffset = absoluteOffset + sizeof(typename ItemDstAdapter::ItemType) * offset_t(testElementIndex);
                const typename ItemDstAdapter::ItemType* testItem =
                    getDirectMemoryAccessUnsafe<typename ItemDstAdapter::ItemType>(testItemAbsoluteOffset);
                ZMEYA_ASSERT(!ItemDstAdapter::eq(*testItem, *lastItem));
            }
#endif
            bucket->endIndex++;
        }
    }

    // assignTo pointer
    template <typename T> static void assignTo(Pointer<T>& dst, std::nullptr_t) { dst.relativeOffset = 0; }

    // assignTo pointer from absolute offset
    template <typename T> void assignTo(Pointer<T>& _dst, offset_t targetAbsoluteOffset)
    {
        BlobPtr<Pointer<T>> dst = getBlobPtr(&_dst);
        roffset_t relativeOffset = toRelativeOffset(diff(targetAbsoluteOffset, dst.getAbsoluteOffset()));
        ZMEYA_ASSERT(relativeOffset != 0);
        dst->relativeOffset = relativeOffset;
    }

    // copyTo pointer from BlobPtr
    template <typename T> void assignTo(Pointer<T>& dst, const BlobPtr<T>& src) { assignTo(dst, src.getAbsoluteOffset()); }

    // copyTo pointer from RawPointer
    template <typename T> void assignTo(Pointer<T>& dst, const T* _src)
    {
        const BlobPtr<T> src = getBlobPtr(_src);
        assignTo(dst, src);
    }

    // copyTo pointer from reference
    template <typename T> void assignTo(Pointer<T>& dst, const T& src) { assignTo(dst, &src); }

    // copyTo array from std::vector
    template <typename T, typename TAllocator> void copyTo(Array<T>& dst, const std::vector<T, TAllocator>& src)
    {
        ZMEYA_ASSERT(src.size() > 0);
        copyToArrayFast(dst, src.data(), src.size());
    }

    // specialization for vector of vectors
    template <typename T, typename TAllocator1, typename TAllocator2>
    void copyTo(Array<Array<T>>& dst, const std::vector<std::vector<T, TAllocator2>, TAllocator1>& src)
    {
        ZMEYA_ASSERT(src.size() > 0);
        copyToArray(dst, src.begin(), src.end(), src.size(),
                    [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const std::vector<T>& src)
                    {
                        Array<T>* dst = blobBuilder->getDirectMemoryAccessUnsafe<Array<T>>(dstAbsoluteOffset);
                        blobBuilder->copyTo(*dst, src);
                    });
    }

    // specialization for vector of strings
    template <typename T, typename TAllocator> void copyTo(Array<String>& dst, const std::vector<T, TAllocator>& src)
    {
        ZMEYA_ASSERT(src.size() > 0);
        copyToArray(dst, src.begin(), src.end(), src.size(),
                    [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const T& src)
                    {
                        String* dst = blobBuilder->getDirectMemoryAccessUnsafe<String>(dstAbsoluteOffset);
                        blobBuilder->copyTo(*dst, src);
                    });
    }

    // copyTo array from std::initializer_list
    template <typename T> void copyTo(Array<T>& dst, std::initializer_list<T> list)
    {
        ZMEYA_ASSERT(list.size() > 0);
        copyToArrayFast(dst, list.begin(), list.size());
    }

    // copyTo array from std::initializer_list
    template <typename T> void copyTo(BlobPtr<Array<T>> dst, std::initializer_list<T> list)
    {
        ZMEYA_ASSERT(list.size() > 0);
        copyToArrayFast(dst, list.begin(), list.size());
    }

    // specialization for std::initializer_list<std::string>
    void copyTo(Array<String>& dst, std::initializer_list<const char*> list)
    {
        ZMEYA_ASSERT(list.size() > 0);
        copyToArray(dst, list.begin(), list.end(), list.size(),
                    [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const char* const& src)
                    {
                        String* dst = blobBuilder->getDirectMemoryAccessUnsafe<String>(dstAbsoluteOffset);
                        blobBuilder->copyTo(*dst, src);
                    });
    }

    // specialization for std::initializer_list<std::string>
    void copyTo(BlobPtr<Array<String>> dst, std::initializer_list<const char*> list)
    {
        ZMEYA_ASSERT(list.size() > 0);
        copyToArray(dst, list.begin(), list.end(), list.size(),
                    [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const char* const& src)
                    {
                        String* dst = blobBuilder->getDirectMemoryAccessUnsafe<String>(dstAbsoluteOffset);
                        blobBuilder->copyTo(*dst, src);
                    });
    }

    // copyTo array from std::array
    template <typename T, size_t NumElements> void copyTo(Array<T>& dst, const std::array<T, NumElements>& src)
    {
        ZMEYA_ASSERT(src.size() > 0);
        copyToArrayFast(dst, src.data(), src.size());
    }

    // specialization for array of strings
    template <typename T, size_t NumElements> void copyTo(Array<String>& dst, const std::array<T, NumElements>& src)
    {
        ZMEYA_ASSERT(src.size() > 0);
        copyToArray(dst, src.begin(), src.end(), src.size(),
                    [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const T& src)
                    {
                        String* dst = blobBuilder->getDirectMemoryAccessUnsafe<String>(dstAbsoluteOffset);
                        blobBuilder->copyTo(*dst, src);
                    });
    }

    // copyTo hash set from std::unordered_set
    template <typename Key, typename Hasher, typename KeyEq, typename TAllocator>
    void copyTo(HashSet<Key>& dst, const std::unordered_set<Key, Hasher, KeyEq, TAllocator>& src)
    {
        typedef HashKeyAdapterGeneric<Key> DstItemAdapter;
        typedef HashKeyAdapterGeneric<Key> SrcItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, src.begin(), src.end(), src.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                *dstElem = srcElem;
            });
    }

    // copyTo hash set from std::unordered_set (specialization for string key)
    template <typename Hasher, typename KeyEq, typename TAllocator>
    void copyTo(HashSet<String>& dst, const std::unordered_set<std::string, Hasher, KeyEq, TAllocator>& src)
    {
        typedef HashKeyAdapterStdString SrcItemAdapter;
        typedef HashKeyAdapterGeneric<String> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, src.begin(), src.end(), src.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                blobBuilder->copyTo(*dstElem, srcElem);
            });
    }

    // copyTo hash set from std::initializer_list
    template <typename Key> void copyTo(HashSet<Key>& dst, std::initializer_list<Key> list)
    {
        typedef HashKeyAdapterGeneric<Key> SrcItemAdapter;
        typedef HashKeyAdapterGeneric<Key> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, list.begin(), list.end(), list.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                *dstElem = srcElem;
            });
    }

    // copyTo hash set from std::initializer_list (specialization for string key)
    void copyTo(HashSet<String>& dst, std::initializer_list<std::string> list)
    {
        typedef HashKeyAdapterStdString SrcItemAdapter;
        typedef HashKeyAdapterGeneric<String> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, list.begin(), list.end(), list.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                blobBuilder->copyTo(*dstElem, srcElem);
            });
    }

    // copyTo hash map from std::unordered_map
    template <typename Key, typename Value, typename Hasher, typename KeyEq, typename TAllocator>
    void copyTo(HashMap<Key, Value>& dst, const std::unordered_map<Key, Value, Hasher, KeyEq, TAllocator>& src)
    {
        typedef HashKeyValueAdapterGeneric<std::pair<const Key, Value>> SrcItemAdapter;
        typedef HashKeyValueAdapterGeneric<Pair<Key, Value>> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, src.begin(), src.end(), src.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                dstElem->first = srcElem.first;
                dstElem->second = srcElem.second;
            });
    }

    // copyTo hash set from std::unordered_map (specialization for string key)
    template <typename Value, typename Hasher, typename KeyEq, typename TAllocator>
    void copyTo(HashMap<String, Value>& dst, const std::unordered_map<std::string, Value, Hasher, KeyEq, TAllocator>& src)
    {
        typedef HashKeyValueAdapterStdString<Value> SrcItemAdapter;
        typedef HashKeyValueAdapterGeneric<Pair<String, Value>> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, src.begin(), src.end(), src.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                dstElem->second = srcElem.second;
                blobBuilder->copyTo(dstElem->first, srcElem.first);
            });
    }

    // copyTo hash set from std::unordered_map (specialization for string value)
    template <typename Key, typename Hasher, typename KeyEq, typename TAllocator>
    void copyTo(HashMap<Key, String>& dst, const std::unordered_map<Key, std::string, Hasher, KeyEq, TAllocator>& src)
    {
        typedef HashKeyValueAdapterGeneric<std::pair<const Key, std::string>> SrcItemAdapter;
        typedef HashKeyValueAdapterGeneric<Pair<Key, String>> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, src.begin(), src.end(), src.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                dstElem->first = srcElem.first;
                blobBuilder->copyTo(dstElem->second, srcElem.second);
            });
    }

    // copyTo hash set from std::unordered_map (specialization for string key/value)
    template <typename Hasher, typename KeyEq, typename TAllocator>
    void copyTo(HashMap<String, String>& dst, const std::unordered_map<std::string, std::string, Hasher, KeyEq, TAllocator>& src)
    {
        typedef HashKeyValueAdapterStdString<std::string> SrcItemAdapter;
        typedef HashKeyValueAdapterGeneric<Pair<String, String>> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, src.begin(), src.end(), src.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                blobBuilder->copyTo(dstElem->first, srcElem.first);

                dstElem = blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                blobBuilder->copyTo(dstElem->second, srcElem.second);
            });
    }

    // copyTo hash map from std::initializer_list
    template <typename Key, typename Value> void copyTo(HashMap<Key, Value>& dst, std::initializer_list<std::pair<const Key, Value>> list)
    {
        typedef HashKeyValueAdapterGeneric<std::pair<const Key, Value>> SrcItemAdapter;
        typedef HashKeyValueAdapterGeneric<Pair<Key, Value>> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, list.begin(), list.end(), list.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                dstElem->first = srcElem.first;
                dstElem->second = srcElem.second;
            });
    }

    // copyTo hash map from std::initializer_list (specialization for string key)
    template <typename Value> void copyTo(HashMap<String, Value>& dst, std::initializer_list<std::pair<const std::string, Value>> list)
    {
        typedef HashKeyValueAdapterStdString<Value> SrcItemAdapter;
        typedef HashKeyValueAdapterGeneric<Pair<String, Value>> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, list.begin(), list.end(), list.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                dstElem->second = srcElem.second;
                blobBuilder->copyTo(dstElem->first, srcElem.first);
            });
    }

    // copyTo hash map from std::initializer_list (specialization for string value)
    template <typename Key> void copyTo(HashMap<Key, String>& dst, std::initializer_list<std::pair<const Key, std::string>> list)
    {
        typedef HashKeyValueAdapterGeneric<std::pair<const Key, std::string>> SrcItemAdapter;
        typedef HashKeyValueAdapterGeneric<Pair<Key, String>> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, list.begin(), list.end(), list.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                dstElem->first = srcElem.first;
                blobBuilder->copyTo(dstElem->second, srcElem.second);
            });
    }

    // copyTo hash map from std::initializer_list (specialization for string key)
    void copyTo(HashMap<String, String>& dst, std::initializer_list<std::pair<const std::string, std::string>> list)
    {
        typedef HashKeyValueAdapterStdString<std::string> SrcItemAdapter;
        typedef HashKeyValueAdapterGeneric<Pair<String, String>> DstItemAdapter;

        copyToHash<SrcItemAdapter, DstItemAdapter>(
            dst, list.begin(), list.end(), list.size(),
            [](BlobBuilder* blobBuilder, offset_t dstAbsoluteOffset, const typename SrcItemAdapter::ItemType& srcElem)
            {
                typename DstItemAdapter::ItemType* dstElem =
                    blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                blobBuilder->copyTo(dstElem->first, srcElem.first);

                dstElem = blobBuilder->getDirectMemoryAccessUnsafe<typename DstItemAdapter::ItemType>(dstAbsoluteOffset);
                blobBuilder->copyTo(dstElem->second, srcElem.second);
            });
    }

    // copyTo string from const char* and size
    void copyTo(BlobPtr<String> dst, const char* src, size_t len)
    {
        ZMEYA_ASSERT(src != nullptr && len > 0);
        BlobPtr<char> stringData = allocate<char>(src[0]);
        if (len > 0)
        {
            for (size_t i = 1; i < len; i++)
            {
                allocate<char>(src[i]);
            }
            allocate<char>('\0');
        }
        assignTo(dst->data, stringData);
    }

    void copyTo(String& _dst, const char* src, size_t len)
    {
        BlobPtr<String> dst(getBlobPtr(&_dst));
        copyTo(dst, src, len);
    }

    // copyTo string from std::string
    void copyTo(String& dst, const std::string& src) { copyTo(dst, src.c_str(), src.size()); }

    // copyTo string from null teminated c-string
    void copyTo(String& _dst, const char* src)
    {
        ZMEYA_ASSERT(src != nullptr);
        size_t len = std::strlen(src);
        copyTo(_dst, src, len);
    }

    void referTo(BlobPtr<String> dst, const String& src)
    {
        BlobPtr<char> stringData = getBlobPtr(src.c_str());
        assignTo(dst->data, stringData);
    }

    // referTo another String (it is not a copy, the destination string will refer to the same data)
    void referTo(String& dst, const String& src)
    {
        BlobPtr<char> stringData = getBlobPtr(src.c_str());
        assignTo(dst.data, stringData);
    }

    // referTo another Array (it is not a copy, the destination string will refer to the same data)
    template <typename T> void referTo(Array<T>& _dst, const Array<T>& src)
    {
        BlobPtr<Array<T>> dst = getBlobPtr(&_dst);
        BlobPtr<T> arrData = getBlobPtr(src.data());
        dst->numElements = uint32_t(src.size());
        setArrayOffset(dst, arrData.getAbsoluteOffset());
    }

    // referTo another HashSet (it is not a copy, the destination string will refer to the same data)
    template <typename Key> void referTo(HashSet<Key>& dst, const HashSet<Key>& src)
    {
        referTo(dst.buckets, src.buckets);
        referTo(dst.items, src.items);
    }

    // referTo another HashMap (it is not a copy, the destination string will refer to the same data)
    template <typename Key, typename Value> void referTo(HashMap<Key, Value>& dst, const HashMap<Key, Value>& src)
    {
        referTo(dst.buckets, src.buckets);
        referTo(dst.items, src.items);
    }

    Span<char> finalize(size_t desiredSizeShouldBeMultipleOf = 4)
    {
        size_t numPaddingBytes = desiredSizeShouldBeMultipleOf - (data.size() % desiredSizeShouldBeMultipleOf);
        allocate(numPaddingBytes, 1);

        ZMEYA_ASSERT((data.size() % desiredSizeShouldBeMultipleOf) == 0);
        return Span<char>(data.data(), data.size());
    }

    ZMEYA_NODISCARD static std::shared_ptr<BlobBuilder> create(size_t initialSizeInBytes = 2048)
    {
        BlobBuilderAllocator<BlobBuilder, ZMEYA_MAX_ALIGN> allocator;
        return std::allocate_shared<BlobBuilder>(allocator, initialSizeInBytes, PrivateToken{});
    }

    template <typename T> friend class BlobPtr;
};

template <typename T> ZMEYA_NODISCARD T* BlobPtr<T>::get() const
{
    std::shared_ptr<const BlobBuilder> p = blob.lock();
    if (!p)
    {
        return nullptr;
    }
    return reinterpret_cast<T*>(const_cast<char*>(p->get(absoluteOffset)));
}

template <typename T> Pointer<T>& Pointer<T>::operator=(const BlobPtr<T>& other)
{
    Pointer<T>& self = *this;
    std::shared_ptr<const BlobBuilder> p = other.blob.lock();
    BlobBuilder* blobBuilder = const_cast<BlobBuilder*>(p.get());
    if (!blobBuilder)
    {
        BlobBuilder::assignTo(self, nullptr);
    }
    else
    {
        blobBuilder->assignTo(self, other);
    }
    return self;
}

#endif
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
