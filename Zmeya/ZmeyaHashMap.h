#pragma once

#include "ZmeyaHashSet.h"

namespace zm
{

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

**Open-addressed hash map (read side)**

`Item` is `Pair<const Key, Value>` so keys stay read-only in the blob while values remain mutable in memory.

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

    ZMEYA_NODISCARD size_t size() const noexcept { return items.size(); }

    ZMEYA_NODISCARD bool empty() const noexcept { return items.empty(); }

    ZMEYA_NODISCARD const Item* begin() const noexcept { return items.begin(); }

    ZMEYA_NODISCARD const Item* end() const noexcept { return items.end(); }

    ZMEYA_NODISCARD bool contains(const Key& key) const noexcept { return find(key) != nullptr; }

    ZMEYA_NODISCARD Value* find(const Key& key) noexcept
    {
        typedef HashKeyAdapterGeneric<Key> Adapter;

        const Value* res = findImpl<Adapter>(key);
        if (!res)
        {
            return nullptr;
        }
        return const_cast<Value*>(res);
    }
    ZMEYA_NODISCARD const Value* find(const Key& key) const noexcept
    {
        typedef HashKeyAdapterGeneric<Key> Adapter;

        return findImpl<Adapter>(key);
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
        if (!res)
        {
            return nullptr;
        }
        return const_cast<Value*>(res);
    }
    ZMEYA_NODISCARD const Value* find(const char* key) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        typedef HashKeyAdapterCStr Adapter;

        return findImpl<Adapter>(key);
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

    template <typename FK, typename FV> HashMap<Key, Value>& operator=(const std::unordered_map<FK, FV>& other)
    {
        assign(*this, other);
        return *this;
    }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    template <typename FK, typename FV> void insert(const FK& key, const FV& value);
    template <typename FK> void erase(const FK& key);
    void clear();
#endif

    template <typename K, typename V, typename FK, typename FV>
    friend void assign(HashMap<K, V>& to, const std::unordered_map<FK, FV>& from);
};

} // namespace zm
