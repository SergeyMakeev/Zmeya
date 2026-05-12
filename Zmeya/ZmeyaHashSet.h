#pragma once

#include "ZmeyaHashAdapters.h"
#include <iterator>

namespace zm
{

/*

**Chained hash set (read side)**

Buckets hold a head index into a dense node pool (`next` links form per-bucket chains). `ZMEYA_HASH_CHAIN_NIL`
marks an absent link or empty bucket. `live_count_` is the number of live elements; `nodes` may hold
extra slots on a free list after erase during serialization.

**Incremental insert / erase** use amortized O(1) chain mutation in the builder (`hashset_chain_*`).
Dense `nodes[]` growth uses `BuilderBase::hash_chain_nodes_array_grow_append_default_hashset` (typed
relocate into a larger slab; string keys use `assign_string_std`, not a full-table `std::unordered_set`
snapshot).

**Invalidation (same idea as `std::unordered_set`)**

During `write_blob` / `BlobWriter`, treat references into this set like an unordered container under
mutation: do not keep raw pointers or iterators across `hashset_insert` / `hashset_erase` / `hashset_clear`
on the same set. Use `w.root()->...` each time. After finalize, the blob is read-only and pointers from
`const` views are stable for that buffer.

*/

static constexpr uint32_t ZMEYA_HASH_CHAIN_NIL = 0xFFFFFFFFu;

template <typename Key> class HashSet
{
  public:
    typedef Key Item;

    friend struct BlobLayoutValidator;

    struct Bucket
    {
        uint32_t head;
    };

    struct Node
    {
        Key key;
        uint32_t next;
    };

    class const_iterator
    {
        const HashSet* m = nullptr;
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
        using value_type = Key;
        using pointer = const Key*;
        using reference = const Key&;

        const_iterator() noexcept = default;

        const_iterator(const HashSet* p, bool at_begin) noexcept
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
            return m->nodes[ni].key;
        }

        pointer operator->() const noexcept
        {
            ZMEYA_DEBUG_ASSERT(m != nullptr && ni != ZMEYA_HASH_CHAIN_NIL && size_t(ni) < m->nodes.size());
            return &m->nodes[ni].key;
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

    template <typename Adapter, typename Key2> ZMEYA_NODISCARD bool containsImpl(const Key2& key) const noexcept
    {
        const size_t numBuckets = buckets.size();
        if (numBuckets == 0)
        {
            return false;
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
                return false;
            }
            if (size_t(i) >= nodePool)
            {
                return false;
            }
            if (Adapter::eq(nodes[i].key, key))
            {
                return true;
            }
        }
        return false;
    }

  public:
    HashSet() noexcept = default;

    ZMEYA_NODISCARD size_t size() const noexcept { return size_t(live_count_); }

    ZMEYA_NODISCARD bool empty() const noexcept { return live_count_ == 0; }

    ZMEYA_NODISCARD const_iterator begin() const noexcept { return const_iterator(this, true); }

    ZMEYA_NODISCARD const_iterator end() const noexcept { return const_iterator(this, false); }

    ZMEYA_NODISCARD bool contains(const char* key) const noexcept
    {
        static_assert(std::is_same<Key, String>::value, "To use this function, the key type must be Zmeya::String");
        return containsImpl<HashKeyAdapterCStr, const char*>(key);
    }

    ZMEYA_NODISCARD bool contains(const Key& key) const noexcept { return containsImpl<HashKeyAdapterGeneric<Key>, Key>(key); }

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

template <typename Key>
struct zm_hashset_chain_incremental_ok : std::true_type
{
};

} // namespace zm
