/*

**Option C chained hash (incremental builder)**

Implements index chaining from `.audit/performance-audit-and-new-design-proposal.md` section 6: dense
`nodes[]`, `uint32_t` links with `ZMEYA_HASH_CHAIN_NIL`, `buckets[].head`, prepend into chains, free list
on erase, rehash when `live_count_ >= bucket_count` (effective max load factor 1.0), initial 8 buckets.
Wire format stays chained through finalize (no flatten-to-flat-items pass).

**Arena growth and `hm` / `hs` references**

`HashMap& hm` and `HashSet& hs` point into `BuilderBase::data`. Any `alloc_aligned` may reallocate that
vector, so those references can dangle for the rest of the call. Always capture `hm_g` / `hs_g` up front,
then re-derive `p` via `get_ptr_unsafe_to_store(hm_g)` after growth. Never call `arena_byte_offset_of(&hm...)`
or `arena_byte_offset_of(&hs...)` after a growth step in the same function unless you re-bound `hm`/`hs`
from `p` (prefer `&p->nodes` etc.).

**Reviewer checklist (after edits)**

- After any step that can grow the arena, rebind `p` with `get_ptr_unsafe_to_store(hm_g)` / `hs_g` before using `hm`/`hs` or addresses derived from them.
- When walking chains, use `p->nodes` / `p->buckets` only from a freshly rebound `p`.

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
    goffset_t hs_g = arena_byte_offset_of(&hs);
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
        note_dead_range(arena_byte_offset_of(oldBuck), oldB * sizeof(typename zm::HashSet<Key>::Bucket));
    }

    p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    p->buckets.numElements = uint32_t(new_num_buckets);
    p->buckets.relativeOffset = get_relative_offset(&p->buckets, newBucketsOffset);
    register_roffset_slot(arena_byte_offset_of(&p->buckets), newBucketsOffset);
}

template <typename K, typename V> void BuilderBase::hashmap_rehash_impl(zm::HashMap<K, V>& hm, size_t new_num_buckets)
{
    ZMEYA_ASSERT(new_num_buckets > 0);
    ZMEYA_ASSERT(new_num_buckets < size_t(std::numeric_limits<uint32_t>::max()));

    // Capture before any alloc in this function; never use &hm after growth here (use hm_g + p).
    goffset_t hm_g = arena_byte_offset_of(&hm);
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
        note_dead_range(arena_byte_offset_of(oldBuck), oldB * sizeof(typename zm::HashMap<K, V>::Bucket));
    }

    p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    p->buckets.numElements = uint32_t(new_num_buckets);
    p->buckets.relativeOffset = get_relative_offset(&p->buckets, newBucketsOffset);
    register_roffset_slot(arena_byte_offset_of(&p->buckets), newBucketsOffset);
}

template <typename Key, typename F> void BuilderBase::hashset_chain_insert(zm::HashSet<Key>& hs, const F& item)
{
    ScopedBuilder tls(this);
    goffset_t hs_g = arena_byte_offset_of(&hs);
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
        register_roffset_slot(arena_byte_offset_of(&p->buckets), buckOfs);
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
        p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    }
    else
    {
        goffset_t nodes_hdr_g = arena_byte_offset_of(&p->nodes);
        hash_chain_nodes_array_grow_append_default_hashset<Key>(
            *reinterpret_cast<Array<typename zm::HashSet<Key>::Node>*>(get_ptr_unsafe_to_store(nodes_hdr_g)));
        p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
        idx = uint32_t(p->nodes.size() - 1);
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
        nodePtr[idx].key = item;
        p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
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
    goffset_t hs_g = arena_byte_offset_of(&hs);
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
    goffset_t hs_g = arena_byte_offset_of(&hs);
    zm::HashSet<Key>* p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    note_dead_hashset_if_any<Key>(p);
    p->live_count_ = 0;
    p->free_head_ = ZMEYA_HASH_CHAIN_NIL;
    p->buckets.numElements = 0;
    p->buckets.relativeOffset = 0;
    p->nodes.numElements = 0;
    p->nodes.relativeOffset = 0;
    register_roffset_slot(arena_byte_offset_of(&p->buckets), goffset_t(0));
    register_roffset_slot(arena_byte_offset_of(&p->nodes), goffset_t(0));
}

template <typename K, typename V, typename FK, typename FV> void BuilderBase::hashmap_chain_insert(zm::HashMap<K, V>& hm, const FK& key, const FV& val)
{
    ScopedBuilder tls(this);
    goffset_t hm_g = arena_byte_offset_of(&hm);
    auto reload_hm = [this, hm_g]() -> zm::HashMap<K, V>* {
        return reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    };
    zm::HashMap<K, V>* p = reload_hm();

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
        p = reload_hm();
        p->buckets.numElements = uint32_t(initialB);
        p->buckets.relativeOffset = get_relative_offset(&p->buckets, buckOfs);
        register_roffset_slot(arena_byte_offset_of(&p->buckets), buckOfs);
    }

    p = reload_hm();
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

    p = reload_hm();
    const size_t B = p->buckets.size();
    if (p->live_count_ >= B)
    {
        const size_t newB = (std::max)(B * 2, size_t(8));
        hashmap_rehash_impl<K, V>(hm, newB);
    }

    p = reload_hm();
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
        p = reload_hm();
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
        nodePtr[idx].value = val;
        p = reload_hm();
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    }
    else
    {
        goffset_t nodes_hdr_g = arena_byte_offset_of(&p->nodes);
        hash_chain_nodes_array_grow_append_default_hashmap<K, V>(
            *reinterpret_cast<Array<typename zm::HashMap<K, V>::Node>*>(get_ptr_unsafe_to_store(nodes_hdr_g)));
        p = reload_hm();
        idx = uint32_t(p->nodes.size() - 1);
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
        nodePtr[idx].key = key;
        p = reload_hm();
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
        nodePtr[idx].value = val;
        p = reload_hm();
        nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
        bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    }

    p = reload_hm();
    nodePtr = p->nodes.get_raw_ptr_unsafe_can_be_relocated();
    bucketPtr = p->buckets.get_raw_ptr_unsafe_can_be_relocated();
    nodePtr[idx].next = bucketPtr[bi].head;
    bucketPtr[bi].head = idx;
    p->live_count_ += 1;
}

template <typename K, typename V, typename FK> void BuilderBase::hashmap_chain_erase(zm::HashMap<K, V>& hm, const FK& key)
{
    ScopedBuilder tls(this);
    goffset_t hm_g = arena_byte_offset_of(&hm);
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
    goffset_t hm_g = arena_byte_offset_of(&hm);
    zm::HashMap<K, V>* p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    note_dead_hashmap_if_any<K, V>(p);
    p->live_count_ = 0;
    p->free_head_ = ZMEYA_HASH_CHAIN_NIL;
    p->buckets.numElements = 0;
    p->buckets.relativeOffset = 0;
    p->nodes.numElements = 0;
    p->nodes.relativeOffset = 0;
    register_roffset_slot(arena_byte_offset_of(&p->buckets), goffset_t(0));
    register_roffset_slot(arena_byte_offset_of(&p->nodes), goffset_t(0));
}

template <typename Key> void BuilderBase::hashset_chain_reserve_node_pool(zm::HashSet<Key>& hs, size_t min_node_pool_capacity)
{
    ScopedBuilder tls(this);
    if (min_node_pool_capacity == 0)
    {
        return;
    }
    ZMEYA_HARD_ASSERT(min_node_pool_capacity <= size_t(std::numeric_limits<uint32_t>::max()) && "node pool capacity exceeds uint32_t");

    goffset_t hs_g = arena_byte_offset_of(&hs);
    zm::HashSet<Key>* p = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(hs_g));
    goffset_t hdr_g = arena_byte_offset_of(&p->nodes);

    if (p->nodes.numElements == 0 && p->nodes.relativeOffset == 0)
    {
        size_t& h = hash_chain_nodes_first_alloc_hint_[hdr_g];
        h = (std::max)(h, min_node_pool_capacity);
        return;
    }

    using Node = typename zm::HashSet<Key>::Node;
    constexpr size_t alignOfNode = std::alignment_of<Node>::value;
    constexpr size_t sizeOfNode = sizeof(Node);

    const size_t old_n = size_t(p->nodes.numElements);
    ZMEYA_ASSERT(old_n > 0);

    size_t old_cap = old_n;
    const auto itPhys = hash_chain_nodes_phys_bytes_.find(hdr_g);
    if (itPhys != hash_chain_nodes_phys_bytes_.end())
    {
        ZMEYA_ASSERT(itPhys->second % sizeOfNode == 0);
        old_cap = itPhys->second / sizeOfNode;
    }
    if (old_cap >= min_node_pool_capacity)
    {
        return;
    }

    ZMEYA_ASSERT(old_n <= SIZE_MAX / 2);
    const size_t new_cap = (std::max)(min_node_pool_capacity, old_cap * 2);
    ZMEYA_ASSERT(new_cap <= SIZE_MAX / sizeOfNode);

    zm::Array<Node>* slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    size_t dead_slab_bytes = old_n * sizeOfNode;
    if (itPhys != hash_chain_nodes_phys_bytes_.end())
    {
        dead_slab_bytes = itPhys->second;
    }
    Node* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
    note_dead_range(arena_byte_offset_of(old_data), dead_slab_bytes);

    goffset_t new_blob_g = alloc_aligned(sizeOfNode * new_cap, alignOfNode);
    slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    for (size_t i = 0; i < old_n; ++i)
    {
        slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
        Node* old_row = slot->get_raw_ptr_unsafe_can_be_relocated();
        const uint32_t saved_next = old_row[i].next;

        Key old_key_copy_trivial{};
        if constexpr (!std::is_same<Key, zm::String>::value)
        {
            old_key_copy_trivial = old_row[i].key;
        }

        Node* new_row = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(i * sizeOfNode)));
        BuilderBase::placementCtor<Key>(&new_row->key);
        if constexpr (std::is_same<Key, zm::String>::value)
        {
            assign_string_cstr(*this, new_row->key, old_row[i].key.c_str());
        }
        else
        {
            new_row->key = old_key_copy_trivial;
        }
        new_row = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(i * sizeOfNode)));
        new_row->next = saved_next;

        slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
        old_row = slot->get_raw_ptr_unsafe_can_be_relocated();
        old_row[i].~Node();
    }

    slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    slot->numElements = uint32_t(old_n);
    slot->relativeOffset = get_relative_offset(slot, new_blob_g);
    register_roffset_slot(hdr_g, new_blob_g);
    hash_chain_nodes_phys_bytes_[hdr_g] = new_cap * sizeOfNode;
}

template <typename K, typename V> void BuilderBase::hashmap_chain_reserve_node_pool(zm::HashMap<K, V>& hm, size_t min_node_pool_capacity)
{
    ScopedBuilder tls(this);
    if (min_node_pool_capacity == 0)
    {
        return;
    }
    ZMEYA_HARD_ASSERT(min_node_pool_capacity <= size_t(std::numeric_limits<uint32_t>::max()) && "node pool capacity exceeds uint32_t");

    goffset_t hm_g = arena_byte_offset_of(&hm);
    zm::HashMap<K, V>* p = reinterpret_cast<zm::HashMap<K, V>*>(get_ptr_unsafe_to_store(hm_g));
    goffset_t hdr_g = arena_byte_offset_of(&p->nodes);

    if (p->nodes.numElements == 0 && p->nodes.relativeOffset == 0)
    {
        size_t& h = hash_chain_nodes_first_alloc_hint_[hdr_g];
        h = (std::max)(h, min_node_pool_capacity);
        return;
    }

    using Node = typename zm::HashMap<K, V>::Node;
    constexpr size_t alignOfNode = std::alignment_of<Node>::value;
    constexpr size_t sizeOfNode = sizeof(Node);

    const size_t old_n = size_t(p->nodes.numElements);
    ZMEYA_ASSERT(old_n > 0);

    size_t old_cap = old_n;
    const auto itPhys = hash_chain_nodes_phys_bytes_.find(hdr_g);
    if (itPhys != hash_chain_nodes_phys_bytes_.end())
    {
        ZMEYA_ASSERT(itPhys->second % sizeOfNode == 0);
        old_cap = itPhys->second / sizeOfNode;
    }
    if (old_cap >= min_node_pool_capacity)
    {
        return;
    }

    ZMEYA_ASSERT(old_n <= SIZE_MAX / 2);
    const size_t new_cap = (std::max)(min_node_pool_capacity, old_cap * 2);
    ZMEYA_ASSERT(new_cap <= SIZE_MAX / sizeOfNode);

    zm::Array<Node>* slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    size_t dead_slab_bytes = old_n * sizeOfNode;
    if (itPhys != hash_chain_nodes_phys_bytes_.end())
    {
        dead_slab_bytes = itPhys->second;
    }
    Node* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
    note_dead_range(arena_byte_offset_of(old_data), dead_slab_bytes);

    goffset_t new_blob_g = alloc_aligned(sizeOfNode * new_cap, alignOfNode);
    slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    for (size_t i = 0; i < old_n; ++i)
    {
        slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
        Node* old_row = slot->get_raw_ptr_unsafe_can_be_relocated();
        const uint32_t saved_next = old_row[i].next;

        K old_key_copy_trivial{};
        V old_val_copy_trivial{};
        if constexpr (!std::is_same<K, zm::String>::value)
        {
            old_key_copy_trivial = old_row[i].key;
        }
        if constexpr (!std::is_same<V, zm::String>::value)
        {
            old_val_copy_trivial = old_row[i].value;
        }

        Node* new_row = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(i * sizeOfNode)));
        BuilderBase::placementCtor<K>(&new_row->key);
        BuilderBase::placementCtor<V>(&new_row->value);
        if constexpr (std::is_same<K, zm::String>::value)
        {
            assign_string_cstr(*this, new_row->key, old_row[i].key.c_str());
            new_row = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(i * sizeOfNode)));
        }
        else
        {
            new_row->key = old_key_copy_trivial;
        }
        if constexpr (std::is_same<V, zm::String>::value)
        {
            assign_string_cstr(*this, new_row->value, old_row[i].value.c_str());
        }
        else
        {
            new_row->value = old_val_copy_trivial;
        }
        new_row = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(i * sizeOfNode)));
        new_row->next = saved_next;

        slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
        old_row = slot->get_raw_ptr_unsafe_can_be_relocated();
        old_row[i].~Node();
    }

    slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    slot->numElements = uint32_t(old_n);
    slot->relativeOffset = get_relative_offset(slot, new_blob_g);
    register_roffset_slot(hdr_g, new_blob_g);
    hash_chain_nodes_phys_bytes_[hdr_g] = new_cap * sizeOfNode;
}
