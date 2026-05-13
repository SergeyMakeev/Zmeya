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
