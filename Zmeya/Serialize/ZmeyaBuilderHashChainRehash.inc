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
