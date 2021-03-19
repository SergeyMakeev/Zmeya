// The MIT License (MIT)
//
// Copyright (c) 2021 Sergey Makeev
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
#include <assert.h>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// This macro is only needed if you want to create serializable data
// (deserialization doesn't need this)
//#define ZMEYA_ENABLE_SERIALIZE_SUPPORT

#define ZMEYA_ASSERT(cond) assert(cond)
#define ZMEYA_NODISCARD [[nodiscard]]
#define ZMEYA_MAX_ALIGN (64)

namespace Zmeya
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
    case 6:
        h ^= (uint64_t)((uint64_t)data2[5] << (uint64_t)40);
    case 5:
        h ^= (uint64_t)((uint64_t)data2[4] << (uint64_t)32);
    case 4:
        h ^= (uint64_t)((uint64_t)data2[3] << (uint64_t)24);
    case 3:
        h ^= (uint64_t)((uint64_t)data2[2] << (uint64_t)16);
    case 2:
        h ^= (uint64_t)((uint64_t)data2[1] << (uint64_t)8);
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

// TODO - description
namespace AppInterop
{

// hashers
template <typename T> ZMEYA_NODISCARD inline size_t hasher(const T& v) { return std::hash<T>{}(v); }

ZMEYA_NODISCARD inline size_t hashString(const char* str)
{
    size_t len = std::strlen(str);
    uint64_t hash = Zmeya::murmur_hash_process64a(str, uint32_t(len), 13061979);
    return size_t(hash);
}

ZMEYA_NODISCARD inline void* aligned_alloc(size_t size, size_t alignment) { return _mm_malloc(size, alignment); }

inline void aligned_free(void* p) { _mm_free(p); }
} // namespace AppInterop

// absolute offset/difference type
typedef uintptr_t offset_t;
typedef ptrdiff_t diff_t;

// relative offset type
typedef int32_t roffset_t;

ZMEYA_NODISCARD inline offset_t toAbsolute(offset_t base, roffset_t offset)
{
    offset_t res = base + diff_t(offset);
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
    roffset_t relativeOffset;

    bool isEqual(const Pointer& other) const noexcept { return get() == other.get(); }

  public:
    Pointer() noexcept = default;

    Pointer& operator=(const Pointer& other) = delete;

    ZMEYA_NODISCARD T* get() const noexcept
    {
        // -1 = this
        // 0 = nullptr (can't point to 1-st byte of this pointer)
        // 1 = this + 2
        //
        // this encoding is used because by default blob memory initialized by zeros and it's convenient to have all pointers initialized to
        // null
        //
        offset_t addr = (relativeOffset == 0) ? offset_t(0) : toAbsolute(offset_t(this) + 1, relativeOffset);
        return reinterpret_cast<T*>(addr);
    }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    Pointer& operator=(const BlobPtr<T>& other);
#endif

    // Note: implicit conversion operator
    operator const T*() const noexcept { return get(); }
    operator T*() noexcept { return get(); }

    ZMEYA_NODISCARD T* operator->() const noexcept { return get(); }

    ZMEYA_NODISCARD T& operator*() const noexcept { return *(get()); }

    operator bool() const noexcept { return relativeOffset != 0; }
    ZMEYA_NODISCARD bool operator==(const Pointer& other) const noexcept { return isEqual(); }
    ZMEYA_NODISCARD bool operator!=(const Pointer& other) const noexcept { return !isEqual(); }

    friend class Blob;
};

/*
    String
*/
class String
{
    Pointer<char> data;

    bool isEqual(const char* s2) const noexcept
    {
        const char* s1 = c_str();
        return (std::strcmp(s1, s2) == 0);
    }

  public:
    String() noexcept = default;

    String& operator=(const String& other) = delete;

    ZMEYA_NODISCARD const char* c_str() const noexcept { return data.get(); }

    ZMEYA_NODISCARD bool empty() const noexcept { return data.get() != nullptr; }
    ZMEYA_NODISCARD bool operator==(const String& other) const noexcept { return isEqual(other.c_str()); }
    ZMEYA_NODISCARD bool operator!=(const String& other) const noexcept { return !isEqual(other.c_str()); }

    ZMEYA_NODISCARD bool operator==(const char* other) const noexcept { return isEqual(other); }
    ZMEYA_NODISCARD bool operator!=(const char* other) const noexcept { return !isEqual(other); }

    friend class Blob;
};

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
        offset_t addr = toAbsolute(offset_t(this), relativeOffset);
        return reinterpret_cast<const T*>(addr);
    }

    ZMEYA_NODISCARD T* getData() const noexcept { return const_cast<T*>(getConstData()); }

  public:
    Array() noexcept = default;

    Array& operator=(const Array& other) = delete;

    ZMEYA_NODISCARD size_t size() const noexcept { return size_t(numElements); }

    ZMEYA_NODISCARD T& operator[](const size_t index) noexcept
    {
        T* data = getData();
        return data[index];
    }

    ZMEYA_NODISCARD const T& operator[](const size_t index) const noexcept
    {
        const T* data = getConstData();
        return data[index];
    }

    ZMEYA_NODISCARD const T* at(const size_t index) const
    {
        ZMEYA_ASSERT(index < size());
        const T* data = getConstData();
        return data[index];
    }

    ZMEYA_NODISCARD T* data() noexcept { return getData(); }
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

    friend class Blob;
};

/*

 Hash adapters

*/

// TODO - rename + update description

// hashset default adapter
template <typename Item> struct HashSetAdapter
{
    static size_t hash(const Item& item) { return AppInterop::hasher(item); }
    static bool eq(const Item& a, const Item& b) { return a == b; }
};

// hashmap default adapter
template <typename Item> struct HashMapAdapter
{
    static size_t hash(const Item& item) { return AppInterop::hasher(item.first); }
    static bool eq(const Item& a, const Item& b) { return a.first == b.first; }
};

// custom String <- const char*
struct HashSetAdapterCStr
{
    static size_t hash(const char* const& key) { return AppInterop::hashString(key); }
    static bool eq(const String& a, const char* const& b) { return a == b; }
};

// hashset std::string
struct HashSetAdapterStdString
{
    static size_t hash(const std::string& key) { return AppInterop::hashString(key.c_str()); }
    static bool eq(const std::string& a, const std::string& b) { return a == b; }
};

// hashmap std::string
template <typename Value> struct HashMapAdapterStdString
{
    typedef std::pair<const std::string, Value> Item;
    static size_t hash(const Item& item) { return AppInterop::hashString(item.first.c_str()); }
    static bool eq(const Item& a, const Item& b) { return a.first == b.first; }
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
            if (Adapter::eq(items[i], key))
            {
                return true;
            }
        }
        return false;
    }

  public:
    HashSet() noexcept = default;

    HashSet& operator=(const HashSet& other) = delete;

    ZMEYA_NODISCARD size_t size() const noexcept { return items.size(); }

    ZMEYA_NODISCARD bool empty() const noexcept { return items.empty(); }

    ZMEYA_NODISCARD const Item* begin() const noexcept { return items.begin(); }

    ZMEYA_NODISCARD const Item* end() const noexcept { return items.end(); }

    ZMEYA_NODISCARD bool contains(const char* key) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        return containsImpl<const char*, HashSetAdapterCStr>(key);
    }

    ZMEYA_NODISCARD bool contains(const Key& key) const noexcept { return containsImpl<Key, HashSetAdapter<Key>>(key); }
    friend class Blob;
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
    typedef Pair<Key, Value> Item;
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
            return false;
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

    HashMap& operator=(const HashMap& other) = delete;

    ZMEYA_NODISCARD size_t size() const noexcept { return items.size(); }

    ZMEYA_NODISCARD bool empty() const noexcept { return items.empty(); }

    ZMEYA_NODISCARD const Item* begin() const noexcept { return items.begin(); }

    ZMEYA_NODISCARD const Item* end() const noexcept { return items.end(); }

    ZMEYA_NODISCARD bool contains(const Key& key) const noexcept { return find(key) != nullptr; }

    ZMEYA_NODISCARD Value* find(const Key& key) noexcept
    {
        typedef HashSetAdapter<Key> Adapter;

        const Value* res = findImpl<Adapter>(key);
        return res ? const_cast<Value*>(res) : nullptr;
    }
    ZMEYA_NODISCARD const Value* find(const Key& key) const noexcept
    {
        typedef HashSetAdapter<Key> Adapter;

        const Value* res = findImpl<Adapter>(key);
        return res;
    }

    ZMEYA_NODISCARD const Value& find(const Key& key, const Value& valueIfNotFound) const noexcept
    {
        typedef HashSetAdapter<Key> Adapter;

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
        typedef HashSetAdapterCStr Adapter;

        const Value* res = findImpl<Adapter>(key);
        return res ? const_cast<Value*>(res) : nullptr;
    }
    ZMEYA_NODISCARD const Value* find(const char* key) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        typedef HashSetAdapterCStr Adapter;

        const Value* res = findImpl<Adapter>(key);
        return res;
    }

    ZMEYA_NODISCARD const Value& find(const char* key, const Value& valueIfNotFound) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        typedef HashSetAdapterCStr Adapter;

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
        typedef HashSetAdapter<Key> Adapter;

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
        typedef HashSetAdapterCStr Adapter;

        const Value* res = findImpl<Adapter>(key);
        if (res)
        {
            return res->c_str();
        }
        return valueIfNotFound;
    }

    friend class Blob;
};

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT

ZMEYA_NODISCARD inline diff_t diff(offset_t a, offset_t b) noexcept
{
    diff_t res = a - b;
    return res;
}

ZMEYA_NODISCARD inline offset_t diff_a(offset_t a, offset_t b)
{
    ZMEYA_ASSERT(a >= b);
    offset_t res = a - b;
    return res;
}

ZMEYA_NODISCARD inline roffset_t toRelativeOffset(diff_t v)
{
    ZMEYA_ASSERT(v >= diff_t(std::numeric_limits<roffset_t>::min()));
    ZMEYA_ASSERT(v <= diff_t(std::numeric_limits<roffset_t>::max()));
    return roffset_t(v);
}

constexpr bool inline isPowerOfTwo(size_t v) { return v && ((v & (v - 1)) == 0); }

class Blob;

/*
    This is a non-serializable pointer to blob internal memory
    Note: blob is able to relocate its own memory that's is why we cannot use
   standard pointers or references
*/
template <typename T> class BlobPtr
{
    std::weak_ptr<const Blob> blob;
    offset_t absoluteOffset = 0;

    bool isEqual(const BlobPtr& other) const
    {
        if (blob != other.blob)
        {
            return false;
        }
        return absoluteOffset == other.absoluteOffset;
    }

  public:
    explicit BlobPtr(std::weak_ptr<const Blob>&& _blob, offset_t _absoluteOffset)
        : blob(std::move(_blob))
        , absoluteOffset(_absoluteOffset)
    {
    }

    BlobPtr() = default;

    BlobPtr(BlobPtr&&) = default;
    BlobPtr& operator=(BlobPtr&&) = default;
    BlobPtr(const BlobPtr&) = default;
    BlobPtr& operator=(const BlobPtr&) = default;

    offset_t getAbsoluteOffset() const { return absoluteOffset; }

    ZMEYA_NODISCARD T* get() const
    {
        std::shared_ptr<const Blob> p = blob.lock();
        if (!p)
        {
            return nullptr;
        }
        return reinterpret_cast<T*>(const_cast<char*>(p->get(absoluteOffset)));
    }

    T* operator->() const { return get(); }

    T& operator*() const { return *(get()); }

    operator bool() const { return get() != nullptr; }

    bool operator==(const BlobPtr& other) const { return isEqual(other); }

    bool operator!=(const BlobPtr& other) const { return !isEqual(other); }

    template <typename T> friend class Pointer;
};

/*
    Blob allocator - aligned allocator for internal Blob usage
*/
template <typename T, int Alignment> class BlobAllocator : public std::allocator<T>
{
  public:
    typedef size_t size_type;
    typedef T* pointer;
    typedef const T* const_pointer;

    template <typename _Tp1> struct rebind
    {
        typedef BlobAllocator<_Tp1, Alignment> other;
    };

    pointer allocate(size_type n)
    {
        void* const pv = AppInterop::aligned_alloc(n * sizeof(T), Alignment);
        return static_cast<pointer>(pv);
    }

    void deallocate(pointer p, size_type) { AppInterop::aligned_free(p); }

    BlobAllocator()
        : std::allocator<T>()
    {
    }
    BlobAllocator(const BlobAllocator& a)
        : std::allocator<T>(a)
    {
    }
    template <class U>
    BlobAllocator(const BlobAllocator<U, Alignment>& a)
        : std::allocator<T>(a)
    {
    }
    ~BlobAllocator() {}
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
    Blob - a binary blob of data that is able to store POD types and special
   "movable" data structures Note: Zmeya containers can be freely moved in
   memory and deserialize from raw bytes without any extra work.
*/
class Blob : public std::enable_shared_from_this<Blob>
{
    std::vector<char, BlobAllocator<char, ZMEYA_MAX_ALIGN>> data;

  private:
    ZMEYA_NODISCARD const char* get(offset_t absoluteOffset) const
    {
        ZMEYA_ASSERT(absoluteOffset < data.size());
        return &data[absoluteOffset];
    }

    template <typename T> ZMEYA_NODISCARD BlobPtr<T> getBlobPtr(const T* p) const
    {
        ZMEYA_ASSERT(containsPointer(p));
        offset_t absoluteOffset = diff_a(offset_t(p), offset_t(data.data()));
        return BlobPtr<T>(weak_from_this(), absoluteOffset);
    }

    struct PrivateToken
    {
    };

  public:
    Blob() = delete;

    Blob(size_t initialSize, PrivateToken)
    {
        static_assert(std::is_trivial<Pointer<int>>::value, "Pointer trivial check failed");
        static_assert(std::is_trivial<Array<int>>::value, "Array trivial check failed");
        static_assert(std::is_trivial<HashSet<int>>::value, "HashSet trivial check failed");
        static_assert(std::is_trivial<Pair<int, float>>::value, "Pair trivial check failed");
        static_assert(std::is_trivial<HashMap<int, int>>::value, "HashMap trivial check failed");
        static_assert(std::is_trivial<String>::value, "String trivial check failed");

        data.reserve(initialSize);
    }

    bool containsPointer(const void* p) const { return (!data.empty() && (p >= &data.front() && p <= &data.back())); }

    template <typename T, typename... _Valty> BlobPtr<T> emplace_back(_Valty&&... _Val)
    {
        // compile time checks
        static_assert(std::is_trivial<T>::value, "Only trivial types allowed");
        constexpr size_t alignOfT = std::alignment_of<T>::value;
        static_assert(isPowerOfTwo(alignOfT), "Non power of two alignment not supported");
        static_assert(alignOfT < ZMEYA_MAX_ALIGN, "Unsupported alignment");

        constexpr size_t sizeOfT = sizeof(T);
        size_t cursor = data.size();

        // padding / alignment
        size_t off = cursor & (alignOfT - 1);
        size_t padding = 0;
        if (off != 0)
        {
            padding = alignOfT - off;
        }
        size_t absoluteOffset = cursor + padding;
        size_t numBytesToAllocate = sizeOfT + padding;

        // allocate more memory (filled with 0, some containers rely on this)
        data.resize(data.size() + numBytesToAllocate, char(0));

        char* p = &data[absoluteOffset];

        // check alignment
        ZMEYA_ASSERT((uintptr_t(p) & (std::alignment_of<T>::value - 1)) == 0);

        // placement new
        ::new (const_cast<void*>(static_cast<const volatile void*>(p))) T(std::forward<_Valty>(_Val)...);

        return BlobPtr<T>(weak_from_this(), absoluteOffset);
    }

    template <typename T> T& getDirectMemoryAccess(size_t absoluteOffset)
    {
        return *const_cast<T*>(reinterpret_cast<const T*>(get(absoluteOffset)));
    }

    // resize array (using custom constructor for new elements)
    template <typename T> size_t resizeArray(Array<T>& _dst, size_t numElements, const T& emptyElement)
    {
        constexpr size_t alignOfT = std::alignment_of<T>::value;
        constexpr size_t sizeOfT = sizeof(T);
        static_assert((sizeOfT % alignOfT) == 0, "The size must be a multiple of the alignment");

        BlobPtr<Array<T>> dst = getBlobPtr(&_dst);
        // An array can be assigned/resized only once (non empty array detected)
        ZMEYA_ASSERT(dst->relativeOffset == 0 && dst->numElements == 0);
        BlobPtr<T> firstElement = emplace_back<T>(emptyElement);
        dst->relativeOffset = toRelativeOffset(diff(firstElement.getAbsoluteOffset(), dst.getAbsoluteOffset()));
        for (size_t i = 1; i < numElements; i++)
        {
            emplace_back<T>(emptyElement);
        }
        ZMEYA_ASSERT(numElements < size_t(std::numeric_limits<uint32_t>::max()));
        dst->numElements = uint32_t(numElements);
        return firstElement.getAbsoluteOffset();
    }

    // assign array from range
    template <typename T, typename Iter, typename ConvertorFunc>
    size_t assignArray(Array<T>& _dst, const Iter begin, const Iter end, int64_t size, ConvertorFunc convertorFunc)
    {
        size_t numElements = (size >= 0) ? size_t(size) : std::distance(begin, end);
        BlobPtr<Array<T>> dst = getBlobPtr(&_dst);
        resizeArray(_dst, numElements);

        BlobPtr<T> firstElement = getBlobPtr(dst->data());
        size_t absoluteOffset = firstElement.getAbsoluteOffset();
        size_t currentIndex = 0;
        for (Iter cur = begin; cur != end; ++cur)
        {
            size_t currentItemAbsoluteOffset = absoluteOffset + sizeof(T) * currentIndex;
            convertorFunc(this, currentItemAbsoluteOffset, *cur);
            currentIndex++;
        }
        return absoluteOffset;
    }

    // assign to hash container
    template <typename SrcItem, typename HashItem, typename HashItemAdapter, typename HashType, typename Iter, typename ConvertorFunc>
    void assignHash(HashType& _dst, Iter begin, Iter end, int64_t size, ConvertorFunc convertorFunc)
    {
        constexpr size_t alignOfT = std::alignment_of<HashItem>::value;
        constexpr size_t sizeOfT = sizeof(HashItem);
        static_assert((sizeOfT % alignOfT) == 0, "The size must be a multiple of the alignment");

        size_t numBuckets = (size >= 0) ? size_t(size) : std::distance(begin, end);
        if (numBuckets == 0)
        {
            return;
        }
        ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
        size_t hashMod = numBuckets;

        // create temporary copy of linearized hash buckets
        typedef std::vector<SrcItem, BlobAllocator<SrcItem, std::alignment_of<SrcItem>::value>> HashBucket;
        std::vector<HashBucket, BlobAllocator<HashBucket, std::alignment_of<HashBucket>::value>> hashBuckets;
        hashBuckets.resize(numBuckets);

        // num unique elements
        size_t numElements = 0;
        for (Iter cur = begin; cur != end; ++cur)
        {
            const auto& current = *cur;
            size_t hash = HashItemAdapter::hash(current);
            size_t bucketIndex = hash % hashMod;
            HashBucket& hashBucket = hashBuckets[bucketIndex];
            size_t numElementsInBucket = hashBucket.size();
            bool isExist = false;
            for (size_t elementIndex = 0; elementIndex < numElementsInBucket; elementIndex++)
            {
                if (HashItemAdapter::eq(hashBucket[elementIndex], current))
                {
                    isExist = true;
                    break;
                }
            }
            if (!isExist)
            {
                hashBucket.emplace_back(*cur);
                numElements++;
            }
        }

        ZMEYA_ASSERT(numElements > 0);

        BlobPtr<HashType> dst = getBlobPtr(&_dst);

        // allocate buckets & items
        resizeArray(dst->buckets, numBuckets);
        size_t absoluteOffset = resizeArray(dst->items, numElements, HashItem());

        // copy linearized items (fill data)
        size_t beginIndex = 0;
        size_t currentIndex = 0;
        for (size_t bucketIndex = 0; bucketIndex < hashBuckets.size(); bucketIndex++)
        {
            const HashBucket& hashBucket = hashBuckets[bucketIndex];
            size_t numElementsInBucket = hashBucket.size();
            for (size_t elementIndex = 0; elementIndex < numElementsInBucket; elementIndex++)
            {
                size_t currentItemAbsoluteOffset = absoluteOffset + sizeof(HashItem) * currentIndex;
                convertorFunc(this, currentItemAbsoluteOffset, hashBucket[elementIndex]);
                currentIndex++;
            }
            ZMEYA_ASSERT(currentIndex < size_t(std::numeric_limits<uint32_t>::max()));

            typename HashType::Bucket& bucket = dst->buckets[bucketIndex];
            bucket.beginIndex = uint32_t(beginIndex);
            bucket.endIndex = uint32_t(currentIndex);
            beginIndex = currentIndex;
        }
    }

    // assign pointer
    template <typename T> static void assign(Pointer<T>& dst, nullptr_t) { dst.relativeOffset = 0; }

    // assign pointer from offset
    template <typename T> void assign(Pointer<T>& _dst, offset_t targetAbsoluteOffset)
    {
        BlobPtr<Pointer<T>> dst = getBlobPtr(&_dst);
        dst->relativeOffset = toRelativeOffset(diff(targetAbsoluteOffset, dst.getAbsoluteOffset()) - 1);
    }

    // assign pointer from BlobPtr
    template <typename T> void assign(Pointer<T>& dst, const BlobPtr<T>& src) { assign(dst, src.getAbsoluteOffset()); }

    // assign pointer from RawPointer
    template <typename T> void assign(Pointer<T>& dst, const T* _src)
    {
        const BlobPtr<T> src = getBlobPtr(_src);
        assign(dst, src);
    }

    // assign pointer from reference
    template <typename T> void assign(Pointer<T>& dst, const T& src)
    {
        assign(dst, &src);
    }


    // assign array from std::vector
    template <typename T, typename TAllocator> void assign(Array<T>& dst, const std::vector<T, TAllocator>& src)
    {
        if (src.empty())
        {
            return;
        }
        assignArray(dst, src.begin(), src.end(), src.size(), [](Blob* blob, size_t dstAbsoluteOffset, const T& src) {
            T& dst = blob->getDirectMemoryAccess<T>(dstAbsoluteOffset);
            dst = src;
        });
    }

    // specialization for vector of strings
    template <typename T, typename TAllocator> void assign(Array<String>& dst, const std::vector<T, TAllocator>& src)
    {
        if (src.empty())
        {
            return;
        }
        assignArray(dst, src.begin(), src.end(), src.size(), [](Blob* blob, size_t dstAbsoluteOffset, const T& src) {
            String& dst = blob->getDirectMemoryAccess<String>(dstAbsoluteOffset);
            blob->assign(dst, src);
        });
    }

    // assign array from std::initializer_list
    template <typename T> void assign(Array<T>& dst, std::initializer_list<T> list)
    {
        if (list.size() == 0)
        {
            return;
        }
        assignArray(dst, list.begin(), list.end(), list.size(), [](Blob* blob, size_t dstAbsoluteOffset, const T& src) {
            T& dst = blob->getDirectMemoryAccess<T>(dstAbsoluteOffset);
            dst = src;
        });
    }

    // specialization for std::initializer_list<std::string>
    void assign(Array<String>& dst, std::initializer_list<const char*> list)
    {
        if (list.size() == 0)
        {
            return;
        }
        assignArray(dst, list.begin(), list.end(), list.size(), [](Blob* blob, size_t dstAbsoluteOffset, const char* const& src) {
            String& dst = blob->getDirectMemoryAccess<String>(dstAbsoluteOffset);
            blob->assign(dst, src);
        });
    }

    // assign array from std::array
    template <typename T, size_t NumElements> void assign(Array<T>& dst, const std::array<T, NumElements>& src)
    {
        if (src.empty())
        {
            return;
        }
        assignArray(dst, src.begin(), src.end(), src.size(), [](Blob* blob, size_t dstAbsoluteOffset, const T& src) {
            T& dst = blob->getDirectMemoryAccess<T>(dstAbsoluteOffset);
            dst = src;
        });
    }

    // specialization for array of strings
    template <typename T, size_t NumElements> void assign(Array<String>& dst, const std::array<T, NumElements>& src)
    {
        if (src.empty())
        {
            return;
        }
        assignArray(dst, src.begin(), src.end(), src.size(), [](Blob* blob, size_t dstAbsoluteOffset, const T& src) {
            String& dst = blob->getDirectMemoryAccess<String>(dstAbsoluteOffset);
            blob->assign(dst, src);
        });
    }

    // resize array (with default constructor)
    template <typename T> void resizeArray(Array<T>& dst, size_t numElements) { resizeArray(dst, numElements, T()); }

    // assign hash set from std::unordered_set
    template <typename Key, typename Hasher, typename KeyEq, typename TAllocator>
    void assign(HashSet<Key>& dst, const std::unordered_set<Key, Hasher, KeyEq, TAllocator>& src)
    {
        typedef Key SrcItem;
        typedef Key HashItem;
        typedef HashSet<Key> HashType;
        typedef HashSetAdapter<Key> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, src.begin(), src.end(), src.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           dst = src;
                                                       });
    }

    // assign hash set from std::unordered_set (specialization for string key)
    template <typename Hasher, typename KeyEq, typename TAllocator>
    void assign(HashSet<String>& dst, const std::unordered_set<std::string, Hasher, KeyEq, TAllocator>& src)
    {
        typedef std::string SrcItem;
        typedef String HashItem;
        typedef HashSet<String> HashType;
        typedef HashSetAdapterStdString HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, src.begin(), src.end(), src.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           blob->assign(dst, src);
                                                       });
    }

    // assign hash set from std::initializer_list
    template <typename Key> void assign(HashSet<Key>& dst, std::initializer_list<Key> list)
    {
        typedef Key SrcItem;
        typedef Key HashItem;
        typedef HashSet<Key> HashType;
        typedef HashSetAdapter<Key> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, list.begin(), list.end(), list.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           dst = src;
                                                       });
    }

    // assign hash map from std::unordered_map
    template <typename Key, typename Value, typename Hasher, typename KeyEq, typename TAllocator>
    void assign(HashMap<Key, Value>& dst, const std::unordered_map<Key, Value, Hasher, KeyEq, TAllocator>& src)
    {
        typedef std::pair<const Key, Value> SrcItem;
        typedef Pair<Key, Value> HashItem;
        typedef HashMap<Key, Value> HashType;
        typedef HashMapAdapter<SrcItem> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, src.begin(), src.end(), src.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           dst.first = src.first;
                                                           dst.second = src.second;
                                                       });
    }

    // assign hash set from std::unordered_map (specialization for string key)
    template <typename Value, typename Hasher, typename KeyEq, typename TAllocator>
    void assign(HashMap<String, Value>& dst, const std::unordered_map<std::string, Value, Hasher, KeyEq, TAllocator>& src)
    {
        typedef std::pair<const std::string, Value> SrcItem;
        typedef Pair<String, Value> HashItem;
        typedef HashMap<String, Value> HashType;
        typedef HashMapAdapterStdString<Value> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, src.begin(), src.end(), src.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           blob->assign(dst.first, src.first);
                                                           dst.second = src.second;
                                                       });
    }

    // assign hash set from std::unordered_map (specialization for string value)
    template <typename Key, typename Hasher, typename KeyEq, typename TAllocator>
    void assign(HashMap<Key, String>& dst, const std::unordered_map<Key, std::string, Hasher, KeyEq, TAllocator>& src)
    {
        typedef std::pair<const Key, std::string> SrcItem;
        typedef Pair<Key, String> HashItem;
        typedef HashMap<Key, String> HashType;
        typedef HashMapAdapter<SrcItem> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, src.begin(), src.end(), src.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           dst.first = src.first;
                                                           blob->assign(dst.second, src.second);
                                                       });
    }

    // assign hash set from std::unordered_map (specialization for string key/value)
    template <typename Hasher, typename KeyEq, typename TAllocator>
    void assign(HashMap<String, String>& dst, const std::unordered_map<std::string, std::string, Hasher, KeyEq, TAllocator>& src)
    {
        typedef std::pair<const std::string, std::string> SrcItem;
        typedef Pair<String, String> HashItem;
        typedef HashMap<String, String> HashType;
        typedef HashMapAdapterStdString<std::string> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, src.begin(), src.end(), src.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           blob->assign(dst.first, src.first);
                                                           blob->assign(dst.second, src.second);
                                                       });
    }

    // assign hash map from std::initializer_list
    template <typename Key, typename Value> void assign(HashMap<Key, Value>& dst, std::initializer_list<std::pair<const Key, Value>> list)
    {
        typedef std::pair<const Key, Value> SrcItem;
        typedef Pair<Key, Value> HashItem;
        typedef HashMap<Key, Value> HashType;
        typedef HashMapAdapter<SrcItem> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, list.begin(), list.end(), list.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           dst.first = src.first;
                                                           dst.second = src.second;
                                                       });
    }

    // assign hash map from std::initializer_list (specialization for string key)
    template <typename Value> void assign(HashMap<String, Value>& dst, std::initializer_list<std::pair<const std::string, Value>> list)
    {
        typedef std::pair<const std::string, Value> SrcItem;
        typedef Pair<String, Value> HashItem;
        typedef HashMap<String, Value> HashType;
        typedef HashMapAdapterStdString<Value> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, list.begin(), list.end(), list.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           blob->assign(dst.first, src.first);
                                                           dst.second = src.second;
                                                       });
    }

    // assign hash map from std::initializer_list (specialization for string value)
    template <typename Key> void assign(HashMap<Key, String>& dst, std::initializer_list<std::pair<const Key, std::string>> list)
    {
        typedef std::pair<const Key, std::string> SrcItem;
        typedef Pair<Key, String> HashItem;
        typedef HashMap<Key, String> HashType;
        typedef HashMapAdapter<SrcItem> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, list.begin(), list.end(), list.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           dst.first = src.first;
                                                           blob->assign(dst.second, src.second);
                                                       });
    }

    // assign hash map from std::initializer_list (specialization for string key)
    void assign(HashMap<String, String>& dst, std::initializer_list<std::pair<const std::string, std::string>> list)
    {
        typedef std::pair<const std::string, std::string> SrcItem;
        typedef Pair<String, String> HashItem;
        typedef HashMap<String, String> HashType;
        typedef HashMapAdapterStdString<std::string> HashItemAdapter;

        assignHash<SrcItem, HashItem, HashItemAdapter>(dst, list.begin(), list.end(), list.size(),
                                                       [](Blob* blob, size_t dstAbsoluteOffset, const SrcItem& src) {
                                                           HashItem& dst = blob->getDirectMemoryAccess<HashItem>(dstAbsoluteOffset);
                                                           blob->assign(dst.first, src.first);
                                                           blob->assign(dst.second, src.second);
                                                       });
    }

    // assign string from const char* and size
    void assign(String& _dst, const char* src, size_t len)
    {
        BlobPtr<String> dst(getBlobPtr(&_dst));
        if (!src)
        {
            assign(dst->data, nullptr);
            return;
        }

        BlobPtr<char> stringData = emplace_back<char>(src[0]);
        if (len > 0)
        {
            for (size_t i = 1; i < len; i++)
            {
                emplace_back<char>(src[i]);
            }
            emplace_back<char>('\0');
        }
        assign(dst->data, stringData);
    }

    // assign string from std::string
    void assign(String& dst, const std::string& src) { assign(dst, src.c_str(), src.size()); }

    // assign string from null teminated c-string
    void assign(String& dst, const char* src)
    {
        size_t len = std::strlen(src);
        assign(dst, src, len);
    }

    Span<char> finalize(size_t desiredSizeShouldBeMultipleOf = 4)
    {
        size_t numPaddingBytes = desiredSizeShouldBeMultipleOf - (data.size() % desiredSizeShouldBeMultipleOf);
        data.resize(data.size() + numPaddingBytes, char(0));

        ZMEYA_ASSERT((data.size() % desiredSizeShouldBeMultipleOf) == 0);
        return Span<char>(data.data(), data.size());
    }

    ZMEYA_NODISCARD static std::shared_ptr<Blob> create(size_t initialSize = 2048)
    {
        return std::make_shared<Blob>(initialSize, PrivateToken{});
    }

    template <typename T> friend class BlobPtr;
};

template <typename T> Pointer<T>& Pointer<T>::operator=(const BlobPtr<T>& other)
{
    Pointer<T>& self = *this;
    std::shared_ptr<const Blob> p = other.blob.lock();
    Blob* blob = const_cast<Blob*>(p.get());
    if (!blob)
    {
        Blob::assign(self, nullptr);
    }
    else
    {
        blob->assign(self, other);
    }
    return self;
}

#endif

} // namespace Zmeya

namespace std
{
template <> struct hash<Zmeya::String>
{
    size_t operator()(Zmeya::String const& s) const noexcept
    {
        const char* str = s.c_str();
        return Zmeya::AppInterop::hashString(str);
    }
};
} // namespace std
