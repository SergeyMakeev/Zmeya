/*

**Option C chained hash (incremental builder)**

Implements index chaining from `.audit/performance-audit-and-new-design-proposal.md` section 6: dense
`nodes[]`, `uint32_t` links with `ZMEYA_HASH_CHAIN_NIL`, `buckets[].head`, prepend into chains, free list
on erase, rehash when `live_count_ >= bucket_count` (effective max load factor 1.0), initial 8 buckets.
Wire format stays chained through finalize (no flatten-to-flat-items pass).

**Arena growth and `hm` / `hs` references**

`HashMap& hm` and `HashSet& hs` point into `BuilderBase::data`. Any `alloc_aligned` may reallocate that
vector, so those references can dangle for the rest of the call. Always capture `hm_g` / `hs_g` up front,
then re-derive `p` via `get_ptr_unsafe_to_store(hm_g)` after growth. Never call `get_global_offset(&hm...)`
or `get_global_offset(&hs...)` after a growth step in the same function unless you re-bound `hm`/`hs`
from `p` (prefer `&p->nodes` etc.).

*/

template <typename Key> inline size_t zm_hash_bucket_index_stored(const Key& k, size_t mod)
{
    if constexpr (std::is_same<Key, zm::String>::value)
    {
        return HashUtils::hashString(k.c_str()) % mod;
    }
    else
    {
        return HashUtils::hasher(k) % mod;
    }
}

template <typename F> inline size_t zm_hash_bucket_index_from_f(const F& f, size_t mod)
{
    if constexpr (std::is_same<F, std::string>::value)
    {
        return HashUtils::hashString(f.c_str()) % mod;
    }
    else
    {
        return HashUtils::hasher(f) % mod;
    }
}

template <typename Key, typename F> inline bool zm_chain_key_eq(const Key& a, const F& b)
{
    if constexpr (std::is_same<Key, zm::String>::value && std::is_same<F, std::string>::value)
    {
        return b == std::string(a.c_str());
    }
    else
    {
        return a == b;
    }
}

template <typename Key> void BuilderBase::hashset_rehash_impl(zm::HashSet<Key>& hs, size_t new_num_buckets)
{
    ZMEYA_ASSERT(new_num_buckets > 0);
    ZMEYA_ASSERT(new_num_buckets < size_t(std::numeric_limits<uint32_t>::max()));

    // Capture before any alloc in this function; never use &hs after growth here (use hs_g + p).
    goffset_t hs_g = get_global_offset(&hs);
    zm::HashSet<Key>* p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));

    std::vector<uint32_t> ord;
    ord.reserve(p->live_count_);
    const size_t oldB = p->buckets.size();
    for (size_t b = 0; b < oldB; ++b)
    {
        uint32_t i = p->buckets[b].head;
        while (i != ZMEYA_HASH_CHAIN_NIL)
        {
            uint32_t nxt = p->nodes[i].next;
            ord.push_back(i);
            i = nxt;
        }
    }
    ZMEYA_ASSERT(ord.size() == p->live_count_);

    constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashSet<Key>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename zm::HashSet<Key>::Bucket);
    zm::goffset_t newBucketsOffset = alloc_aligned(sizeOfBucket * new_num_buckets, alignOfBucket);
    typename zm::HashSet<Key>::Bucket* newBuckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(newBucketsOffset));
    for (size_t i = 0; i < new_num_buckets; ++i)
    {
        BuilderBase::placementCtor<typename zm::HashSet<Key>::Bucket>(&newBuckets[i], typename zm::HashSet<Key>::Bucket{ZMEYA_HASH_CHAIN_NIL});
    }

    p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    typename zm::HashSet<Key>::Node* nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    for (uint32_t idx : ord)
    {
        const size_t bi = zm_hash_bucket_index_stored(nodePtr[idx].key, new_num_buckets);
        nodePtr[idx].next = newBuckets[bi].head;
        newBuckets[bi].head = idx;
    }

    p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    if (oldB > 0)
    {
        typename zm::HashSet<Key>::Bucket* oldBuck = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(p->buckets.get_raw_ptr_unsafe_can_be_relocated());
        note_dead_range(get_global_offset(oldBuck), oldB * sizeof(typename zm::HashSet<Key>::Bucket));
    }

    p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    p->buckets.numElements = uint32_t(new_num_buckets);
    p->buckets.relativeOffset = get_relative_offset(&p->buckets, newBucketsOffset);
    register_roffset_slot(get_global_offset(&p->buckets), newBucketsOffset);
}

template <typename K, typename V> void BuilderBase::hashmap_rehash_impl(zm::HashMap<K, V>& hm, size_t new_num_buckets)
{
    ZMEYA_ASSERT(new_num_buckets > 0);
    ZMEYA_ASSERT(new_num_buckets < size_t(std::numeric_limits<uint32_t>::max()));

    // Capture before any alloc in this function; never use &hm after growth here (use hm_g + p).
    goffset_t hm_g = get_global_offset(&hm);
    zm::HashMap<K, V>* p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));

    std::vector<uint32_t> ord;
    ord.reserve(p->live_count_);
    const size_t oldB = p->buckets.size();
    for (size_t b = 0; b < oldB; ++b)
    {
        uint32_t i = p->buckets[b].head;
        while (i != ZMEYA_HASH_CHAIN_NIL)
        {
            uint32_t nxt = p->nodes[i].next;
            ord.push_back(i);
            i = nxt;
        }
    }
    ZMEYA_ASSERT(ord.size() == p->live_count_);

    constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashMap<K, V>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename zm::HashMap<K, V>::Bucket);
    zm::goffset_t newBucketsOffset = alloc_aligned(sizeOfBucket * new_num_buckets, alignOfBucket);
    typename zm::HashMap<K, V>::Bucket* newBuckets = reinterpret_cast<typename zm::HashMap<K, V>::Bucket*>(get_ptr_unsafe_to_store(newBucketsOffset));
    for (size_t i = 0; i < new_num_buckets; ++i)
    {
        BuilderBase::placementCtor<typename zm::HashMap<K, V>::Bucket>(&newBuckets[i], typename zm::HashMap<K, V>::Bucket{ZMEYA_HASH_CHAIN_NIL});
    }

    p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    typename zm::HashMap<K, V>::Node* nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    for (uint32_t idx : ord)
    {
        const size_t bi = zm_hash_bucket_index_stored(nodePtr[idx].key, new_num_buckets);
        nodePtr[idx].next = newBuckets[bi].head;
        newBuckets[bi].head = idx;
    }

    p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    if (oldB > 0)
    {
        typename zm::HashMap<K, V>::Bucket* oldBuck = reinterpret_cast<typename zm::HashMap<K, V>::Bucket*>(p->buckets.get_raw_ptr_unsafe_can_be_relocated());
        note_dead_range(get_global_offset(oldBuck), oldB * sizeof(typename zm::HashMap<K, V>::Bucket));
    }

    p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    p->buckets.numElements = uint32_t(new_num_buckets);
    p->buckets.relativeOffset = get_relative_offset(&p->buckets, newBucketsOffset);
    register_roffset_slot(get_global_offset(&p->buckets), newBucketsOffset);
}

template <typename Key, typename F> void BuilderBase::hashset_chain_insert(zm::HashSet<Key>& hs, const F& item)
{
    ScopedBuilder tls(this);
    goffset_t hs_g = get_global_offset(&hs);
    zm::HashSet<Key>* p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));

    if (p->buckets.size() == 0)
    {
        constexpr size_t initialB = 8;
        constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashSet<Key>::Bucket>::value;
        constexpr size_t sizeOfBucket = sizeof(typename zm::HashSet<Key>::Bucket);
        zm::goffset_t buckOfs = alloc_aligned(sizeOfBucket * initialB, alignOfBucket);
        typename zm::HashSet<Key>::Bucket* buckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(buckOfs));
        for (size_t i = 0; i < initialB; ++i)
        {
            BuilderBase::placementCtor<typename zm::HashSet<Key>::Bucket>(&buckets[i], typename zm::HashSet<Key>::Bucket{ZMEYA_HASH_CHAIN_NIL});
        }
        p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
        p->buckets.numElements = uint32_t(initialB);
        p->buckets.relativeOffset = get_relative_offset(&p->buckets, buckOfs);
        register_roffset_slot(get_global_offset(&p->buckets), buckOfs);
    }

    p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    const size_t B = p->buckets.size();
    if (p->live_count_ >= B)
    {
        const size_t newB = (std::max)(B * 2, size_t(8));
        hashset_rehash_impl<Key>(hs, newB);
    }

    p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    const size_t B2 = p->buckets.size();
    const size_t bi = zm_hash_bucket_index_from_f(item, B2);

    typename zm::HashSet<Key>::Node* nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    typename zm::HashSet<Key>::Bucket* bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();

    uint32_t idx = ZMEYA_HASH_CHAIN_NIL;
    if (p->free_head_ != ZMEYA_HASH_CHAIN_NIL)
    {
        idx = p->free_head_;
        p->free_head_ = nodePtr[idx].next;
        nodePtr[idx].key = item;
        nodePtr[idx].next = ZMEYA_HASH_CHAIN_NIL;
    }
    else
    {
        typename zm::HashSet<Key>::Node node;
        BuilderBase::placementCtor<Key>(&node.key);
        node.key = item;
        node.next = ZMEYA_HASH_CHAIN_NIL;
        // `hs` may dangle after bucket init alloc above; use p from hs_g for any member address.
        p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
        goffset_t nodes_hdr_g = get_global_offset(&p->nodes);
        array_push_back(*reinterpret_cast<Array<typename zm::HashSet<Key>::Node>*>(get_ptr_unsafe_to_store(nodes_hdr_g)), node);
        p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
        idx = uint32_t(p->nodes.size() - 1);
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    }

    p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    nodePtr[idx].next = bucketPtr[bi].head;
    bucketPtr[bi].head = idx;
    p->live_count_ += 1;
}

template <typename Key, typename F> void BuilderBase::hashset_chain_erase(zm::HashSet<Key>& hs, const F& key)
{
    ScopedBuilder tls(this);
    goffset_t hs_g = get_global_offset(&hs);
    zm::HashSet<Key>* p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    const size_t B = p->buckets.size();
    if (B == 0 || p->live_count_ == 0)
    {
        return;
    }
    const size_t bi = zm_hash_bucket_index_from_f(key, B);

    typename zm::HashSet<Key>::Node* nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    typename zm::HashSet<Key>::Bucket* bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();

    uint32_t prev = ZMEYA_HASH_CHAIN_NIL;
    uint32_t cur = bucketPtr[bi].head;
    while (cur != ZMEYA_HASH_CHAIN_NIL)
    {
        if (zm_chain_key_eq(nodePtr[cur].key, key))
        {
            const uint32_t nxt = nodePtr[cur].next;
            if (prev == ZMEYA_HASH_CHAIN_NIL)
            {
                bucketPtr[bi].head = nxt;
            }
            else
            {
                nodePtr[prev].next = nxt;
            }
            nodePtr[cur].next = p->free_head_;
            p->free_head_ = cur;
            ZMEYA_ASSERT(p->live_count_ > 0);
            p->live_count_ -= 1;
            return;
        }
        prev = cur;
        cur = nodePtr[cur].next;
    }
}

template <typename Key> void BuilderBase::hashset_chain_clear(zm::HashSet<Key>& hs)
{
    ScopedBuilder tls(this);
    goffset_t hs_g = get_global_offset(&hs);
    zm::HashSet<Key>* p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    note_dead_hashset_if_any<Key>(p);
    p->live_count_ = 0;
    p->free_head_ = ZMEYA_HASH_CHAIN_NIL;
    p->buckets.numElements = 0;
    p->buckets.relativeOffset = 0;
    p->nodes.numElements = 0;
    p->nodes.relativeOffset = 0;
    register_roffset_slot(get_global_offset(&p->buckets), goffset_t(0));
    register_roffset_slot(get_global_offset(&p->nodes), goffset_t(0));
}

template <typename K, typename V, typename FK, typename FV> void BuilderBase::hashmap_chain_insert(zm::HashMap<K, V>& hm, const FK& key, const FV& val)
{
    ScopedBuilder tls(this);
    goffset_t hm_g = get_global_offset(&hm);
    zm::HashMap<K, V>* p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));

    if (p->buckets.size() == 0)
    {
        constexpr size_t initialB = 8;
        constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashMap<K, V>::Bucket>::value;
        constexpr size_t sizeOfBucket = sizeof(typename zm::HashMap<K, V>::Bucket);
        zm::goffset_t buckOfs = alloc_aligned(sizeOfBucket * initialB, alignOfBucket);
        typename zm::HashMap<K, V>::Bucket* buckets = reinterpret_cast<typename zm::HashMap<K, V>::Bucket*>(get_ptr_unsafe_to_store(buckOfs));
        for (size_t i = 0; i < initialB; ++i)
        {
            BuilderBase::placementCtor<typename zm::HashMap<K, V>::Bucket>(&buckets[i], typename zm::HashMap<K, V>::Bucket{ZMEYA_HASH_CHAIN_NIL});
        }
        p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
        p->buckets.numElements = uint32_t(initialB);
        p->buckets.relativeOffset = get_relative_offset(&p->buckets, buckOfs);
        register_roffset_slot(get_global_offset(&p->buckets), buckOfs);
    }

    p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    const size_t B0 = p->buckets.size();
    const size_t probeBi = zm_hash_bucket_index_from_f(key, B0);
    typename zm::HashMap<K, V>::Node* nodePtr0 = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    typename zm::HashMap<K, V>::Bucket* bucketPtr0 = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    for (uint32_t i = bucketPtr0[probeBi].head; i != ZMEYA_HASH_CHAIN_NIL; i = nodePtr0[i].next)
    {
        if (zm_chain_key_eq(nodePtr0[i].key, key))
        {
            nodePtr0[i].value = val;
            return;
        }
    }

    p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    const size_t B = p->buckets.size();
    if (p->live_count_ >= B)
    {
        const size_t newB = (std::max)(B * 2, size_t(8));
        hashmap_rehash_impl<K, V>(hm, newB);
    }

    p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    const size_t B2 = p->buckets.size();
    const size_t bi = zm_hash_bucket_index_from_f(key, B2);

    typename zm::HashMap<K, V>::Node* nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    typename zm::HashMap<K, V>::Bucket* bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();

    uint32_t idx = ZMEYA_HASH_CHAIN_NIL;
    if (p->free_head_ != ZMEYA_HASH_CHAIN_NIL)
    {
        idx = p->free_head_;
        p->free_head_ = nodePtr[idx].next;
        nodePtr[idx].key = key;
        nodePtr[idx].value = val;
        nodePtr[idx].next = ZMEYA_HASH_CHAIN_NIL;
    }
    else
    {
        typename zm::HashMap<K, V>::Node node;
        BuilderBase::placementCtor<K>(&node.key);
        BuilderBase::placementCtor<V>(&node.value);
        node.key = key;
        node.value = val;
        node.next = ZMEYA_HASH_CHAIN_NIL;
        // `hm` may dangle after bucket init alloc above; use p from hm_g for any member address.
        p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
        goffset_t nodes_hdr_g = get_global_offset(&p->nodes);
        array_push_back(*reinterpret_cast<Array<typename zm::HashMap<K, V>::Node>*>(get_ptr_unsafe_to_store(nodes_hdr_g)), node);
        p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
        idx = uint32_t(p->nodes.size() - 1);
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    }

    p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    nodePtr[idx].next = bucketPtr[bi].head;
    bucketPtr[bi].head = idx;
    p->live_count_ += 1;
}

template <typename K, typename V, typename FK> void BuilderBase::hashmap_chain_erase(zm::HashMap<K, V>& hm, const FK& key)
{
    ScopedBuilder tls(this);
    goffset_t hm_g = get_global_offset(&hm);
    zm::HashMap<K, V>* p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    const size_t B = p->buckets.size();
    if (B == 0 || p->live_count_ == 0)
    {
        return;
    }
    const size_t bi = zm_hash_bucket_index_from_f(key, B);

    typename zm::HashMap<K, V>::Node* nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    typename zm::HashMap<K, V>::Bucket* bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();

    uint32_t prev = ZMEYA_HASH_CHAIN_NIL;
    uint32_t cur = bucketPtr[bi].head;
    while (cur != ZMEYA_HASH_CHAIN_NIL)
    {
        if (zm_chain_key_eq(nodePtr[cur].key, key))
        {
            const uint32_t nxt = nodePtr[cur].next;
            if (prev == ZMEYA_HASH_CHAIN_NIL)
            {
                bucketPtr[bi].head = nxt;
            }
            else
            {
                nodePtr[prev].next = nxt;
            }
            nodePtr[cur].next = p->free_head_;
            p->free_head_ = cur;
            ZMEYA_ASSERT(p->live_count_ > 0);
            p->live_count_ -= 1;
            return;
        }
        prev = cur;
        cur = nodePtr[cur].next;
    }
}

template <typename K, typename V> void BuilderBase::hashmap_chain_clear(zm::HashMap<K, V>& hm)
{
    ScopedBuilder tls(this);
    goffset_t hm_g = get_global_offset(&hm);
    zm::HashMap<K, V>* p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    note_dead_hashmap_if_any<K, V>(p);
    p->live_count_ = 0;
    p->free_head_ = ZMEYA_HASH_CHAIN_NIL;
    p->buckets.numElements = 0;
    p->buckets.relativeOffset = 0;
    p->nodes.numElements = 0;
    p->nodes.relativeOffset = 0;
    register_roffset_slot(get_global_offset(&p->buckets), goffset_t(0));
    register_roffset_slot(get_global_offset(&p->nodes), goffset_t(0));
}
