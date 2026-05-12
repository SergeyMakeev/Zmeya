#pragma once

#include "ZmeyaHashSet.h"
#include <iterator>
#include <utility>

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

**Chained hash map (read side)**

Same bucket + index chain model as `HashSet`. Each `Node` stores `key` and `value` separately so the table
can be built incrementally without a default-constructed `Pair<const Key, Value>`.

Iteration exposes `std::pair<const Key&, const Value&>` so `zm::String` keys are not shallow-copied (their
`Pointer` offsets are relative to the slot address inside the blob).

**Invalidation (same idea as `std::unordered_map`)**

During `write_scope` / `BlobWriter`, do not keep raw pointers or iterators across `hashmap_insert` /
`hashmap_erase` / `hashmap_clear` on the same map. Use `w.root()->...` each time. After finalize, the
blob is read-only and pointers from `const` views are stable for that buffer.

**Lookup complexity**

`find` walks one bucket chain. Average probe depth stays O(1) for well-spread keys at the write path
load factor (up to 1.0 before rehash). Worst-case clustering is Theta(n) per lookup; `findImpl` caps
walks at `nodes.size() + 1` steps and returns null if that bound is exceeded.

*/

template <typename Key, typename Value> class HashMap
{
    typedef Pair<const Key, Value> Item;

    friend struct BlobLayoutValidator;

    struct Bucket
    {
        uint32_t head;
    };

    struct Node
    {
        Key key;
        Value value;
        uint32_t next;
    };

    class const_iterator
    {
        const HashMap* m = nullptr;
        size_t bi = 0;
        uint32_t ni = ZMEYA_HASH_CHAIN_NIL;

        void mark_end() noexcept
        {
            if (!m)
            {
                return;
            }
            bi = m->buckets.size();
            ni = ZMEYA_HASH_CHAIN_NIL;
        }

        void seek_first() noexcept
        {
            if (!m || m->buckets.size() == 0)
            {
                mark_end();
                return;
            }
            if (m->empty())
            {
                mark_end();
                return;
            }
            for (bi = 0; bi < m->buckets.size(); ++bi)
            {
                ni = m->buckets[bi].head;
                if (ni != ZMEYA_HASH_CHAIN_NIL && size_t(ni) < m->nodes.size())
                {
                    return;
                }
            }
            mark_end();
        }

      public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Item;
        using reference = std::pair<const Key&, const Value&>;

        const_iterator() noexcept = default;

        const_iterator(const HashMap* p, bool at_begin) noexcept
            : m(p)
        {
            if (at_begin)
            {
                seek_first();
            }
            else
            {
                mark_end();
            }
        }

        reference operator*() const noexcept
        {
            ZMEYA_DEBUG_ASSERT(m != nullptr && ni != ZMEYA_HASH_CHAIN_NIL && size_t(ni) < m->nodes.size());
            return reference(m->nodes[ni].key, m->nodes[ni].value);
        }

        const_iterator& operator++() noexcept
        {
            if (!m)
            {
                return *this;
            }
            if (ni != ZMEYA_HASH_CHAIN_NIL)
            {
                if (size_t(ni) >= m->nodes.size())
                {
                    mark_end();
                    return *this;
                }
                const uint32_t nxt = m->nodes[ni].next;
                if (nxt != ZMEYA_HASH_CHAIN_NIL && size_t(nxt) >= m->nodes.size())
                {
                    mark_end();
                    return *this;
                }
                ni = nxt;
                if (ni != ZMEYA_HASH_CHAIN_NIL)
                {
                    return *this;
                }
                ++bi;
            }
            for (; bi < m->buckets.size(); ++bi)
            {
                ni = m->buckets[bi].head;
                if (ni != ZMEYA_HASH_CHAIN_NIL && size_t(ni) < m->nodes.size())
                {
                    return *this;
                }
            }
            mark_end();
            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            const_iterator t = *this;
            ++*this;
            return t;
        }

        bool operator==(const const_iterator& o) const noexcept { return m == o.m && bi == o.bi && ni == o.ni; }

        bool operator!=(const const_iterator& o) const noexcept { return !(*this == o); }
    };

    using iterator = const_iterator;

  private:
    uint32_t live_count_ = 0;
    uint32_t free_head_ = ZMEYA_HASH_CHAIN_NIL;
    Array<Bucket> buckets;
    Array<Node> nodes;

    friend class detail::BuilderBase;

    template <typename Adapter, typename Key2> ZMEYA_NODISCARD const Value* findImpl(const Key2& key) const noexcept
    {
        const size_t numBuckets = buckets.size();
        if (numBuckets == 0)
        {
            return nullptr;
        }
        const size_t hashMod = numBuckets;
        const size_t hash = Adapter::hash(key);
        const size_t bucketIndex = hash % hashMod;
        const size_t nodePool = nodes.size();
        const size_t maxSteps = nodePool + 1;
        size_t steps = 0;
        for (uint32_t i = buckets[bucketIndex].head; i != ZMEYA_HASH_CHAIN_NIL; i = nodes[i].next)
        {
            if (++steps > maxSteps)
            {
                return nullptr;
            }
            if (size_t(i) >= nodePool)
            {
                return nullptr;
            }
            if (Adapter::eq(nodes[i].key, key))
            {
                return &nodes[i].value;
            }
        }
        return nullptr;
    }

  public:
    HashMap() noexcept = default;

    ZMEYA_NODISCARD size_t size() const noexcept { return size_t(live_count_); }

    ZMEYA_NODISCARD bool empty() const noexcept { return live_count_ == 0; }

    ZMEYA_NODISCARD const_iterator begin() const noexcept { return const_iterator(this, true); }

    ZMEYA_NODISCARD const_iterator end() const noexcept { return const_iterator(this, false); }

    ZMEYA_NODISCARD bool contains(const Key& key) const noexcept { return find(key) != nullptr; }

    ZMEYA_NODISCARD Value* find(const Key& key) noexcept
    {
        typedef HashKeyAdapterGeneric<Key> Adapter;

        const Value* res = findImpl<Adapter, Key>(key);
        if (!res)
        {
            return nullptr;
        }
        return const_cast<Value*>(res);
    }
    ZMEYA_NODISCARD const Value* find(const Key& key) const noexcept
    {
        typedef HashKeyAdapterGeneric<Key> Adapter;

        return findImpl<Adapter, Key>(key);
    }

    ZMEYA_NODISCARD const Value& find(const Key& key, const Value& valueIfNotFound) const noexcept
    {
        typedef HashKeyAdapterGeneric<Key> Adapter;

        const Value* res = findImpl<Adapter, Key>(key);
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

        const Value* res = findImpl<Adapter, const char*>(key);
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

        return findImpl<Adapter, const char*>(key);
    }

    ZMEYA_NODISCARD const Value& find(const char* key, const Value& valueIfNotFound) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        typedef HashKeyAdapterCStr Adapter;

        const Value* res = findImpl<Adapter, const char*>(key);
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

        const Value* res = findImpl<Adapter, Key>(key);
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

        const Value* res = findImpl<Adapter, const char*>(key);
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

template <typename Key, typename Value>
struct zm_hashmap_chain_incremental_ok : std::true_type
{
};

} // namespace zm
