#pragma once

#include "ZmeyaHashAdapters.h"

namespace zm
{

/*

**Open-addressed hash set (read side)**

Buckets store index ranges into a flat `items` array. Serialization fills the same layout from STL sets.

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

    template <typename F> HashSet<Key>& operator=(const std::unordered_set<F>& other)
    {
        assign(*this, other);
        return *this;
    }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    template <typename F> void insert(const F& item);
    template <typename F> void erase(const F& key);
    void clear();
#endif

    template <typename K, typename F> friend void assign(HashSet<K>& to, const std::unordered_set<F>& from);
};

} // namespace zm
