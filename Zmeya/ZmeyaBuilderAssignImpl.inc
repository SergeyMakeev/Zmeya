template <typename T> void BuilderBase::impl_assign_pointer_array(zm::Array<zm::Pointer<T>>& _to, const std::vector<T*>& from)
{
    goffset_t to_offset = get_global_offset(&_to);
    if (from.empty())
    {
        zm::Array<zm::Pointer<T>>* to = reinterpret_cast<zm::Array<zm::Pointer<T>>*>(get_ptr_unsafe_to_store(to_offset));
        if (to->numElements != 0 || to->relativeOffset != 0)
        {
            note_dead_array_slab_if_any<zm::Pointer<T>>(to);
            to->numElements = 0;
            to->relativeOffset = 0;
            register_roffset_slot(to_offset, goffset_t(0));
        }
        return;
    }
    zm::Array<zm::Pointer<T>>* to = reinterpret_cast<zm::Array<zm::Pointer<T>>*>(get_ptr_unsafe_to_store(to_offset));
    if (to->numElements != 0 || to->relativeOffset != 0)
    {
        note_dead_array_slab_if_any<zm::Pointer<T>>(to);
        to->numElements = 0;
        to->relativeOffset = 0;
        register_roffset_slot(to_offset, goffset_t(0));
        to = reinterpret_cast<zm::Array<zm::Pointer<T>>*>(get_ptr_unsafe_to_store(to_offset));
    }
    constexpr size_t alignOfPtr = std::alignment_of<zm::Pointer<T>>::value;
    constexpr size_t sizeOfPtr = sizeof(zm::Pointer<T>);
    zm::goffset_t arrayDataOffset = alloc_aligned(sizeOfPtr * from.size(), alignOfPtr);
    to = reinterpret_cast<zm::Array<zm::Pointer<T>>*>(get_ptr_unsafe_to_store(to_offset));
    to->numElements = uint32_t(from.size());
    to->relativeOffset = get_relative_offset(to, arrayDataOffset);
    register_roffset_slot(to_offset, arrayDataOffset);
    for (size_t i = 0; i < from.size(); ++i)
    {
        zm::goffset_t elementOffset = zm::goffset_t(arrayDataOffset + i * sizeOfPtr);
        zm::Pointer<T>* element = reinterpret_cast<zm::Pointer<T>*>(get_ptr_unsafe_to_store(elementOffset));
        BuilderBase::placementCtor<zm::Pointer<T>>(element);
        assign_pointer(*this, *element, from[i]);
    }
}

template <typename T, typename F> void BuilderBase::impl_assign_array_vector(zm::Array<T>& _to, const std::vector<F>& from)
{
    goffset_t to_offset = get_global_offset(&_to);
    if (from.empty())
    {
        zm::Array<T>* to = reinterpret_cast<zm::Array<T>*>(get_ptr_unsafe_to_store(to_offset));
        if (to->numElements != 0 || to->relativeOffset != 0)
        {
            note_dead_array_slab_if_any<T>(to);
            to->numElements = 0;
            to->relativeOffset = 0;
            register_roffset_slot(to_offset, goffset_t(0));
        }
        return;
    }
    zm::Array<T>* to = reinterpret_cast<zm::Array<T>*>(get_ptr_unsafe_to_store(to_offset));
    if (to->numElements != 0 || to->relativeOffset != 0)
    {
        note_dead_array_slab_if_any<T>(to);
        to->numElements = 0;
        to->relativeOffset = 0;
        register_roffset_slot(to_offset, goffset_t(0));
        to = reinterpret_cast<zm::Array<T>*>(get_ptr_unsafe_to_store(to_offset));
    }
    constexpr size_t alignOfT = std::alignment_of<T>::value;
    constexpr size_t sizeOfT = sizeof(T);
    zm::goffset_t arrayDataOffset = alloc_aligned(sizeOfT * from.size(), alignOfT);
    to = reinterpret_cast<zm::Array<T>*>(get_ptr_unsafe_to_store(to_offset));
    to->numElements = uint32_t(from.size());
    to->relativeOffset = get_relative_offset(to, arrayDataOffset);
    register_roffset_slot(to_offset, arrayDataOffset);
    for (size_t i = 0; i < from.size(); ++i)
    {
        zm::goffset_t elementOffset = zm::goffset_t(arrayDataOffset + i * sizeOfT);
        zm::deep_copy<F, T>(*this, from[i], elementOffset);
    }
}

template <typename Key, typename F> void BuilderBase::impl_assign_hashset(zm::HashSet<Key>& _to, const std::unordered_set<F>& from)
{
    ScopedBuilder tls(this);
    goffset_t to_offset = get_global_offset(&_to);
    zm::HashSet<Key>* to = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(to_offset));
    note_dead_hashset_if_any<Key>(to);
    if (from.empty())
    {
        to->live_count_ = 0;
        to->free_head_ = ZMEYA_HASH_CHAIN_NIL;
        to->buckets.numElements = 0;
        to->buckets.relativeOffset = 0;
        to->nodes.numElements = 0;
        to->nodes.relativeOffset = 0;
        register_roffset_slot(get_global_offset(&to->buckets), goffset_t(0));
        register_roffset_slot(get_global_offset(&to->nodes), goffset_t(0));
        return;
    }

    const size_t numElements = from.size();
    size_t numBuckets = numElements * 2;
    ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
    if (numBuckets == 0)
    {
        numBuckets = 1;
    }
    const size_t hashMod = numBuckets;

    constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashSet<Key>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename zm::HashSet<Key>::Bucket);
    zm::goffset_t bucketsDataOffset = alloc_aligned(sizeOfBucket * numBuckets, alignOfBucket);

    typename zm::HashSet<Key>::Bucket* buckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
    for (size_t i = 0; i < numBuckets; ++i)
    {
        BuilderBase::placementCtor<typename zm::HashSet<Key>::Bucket>(&buckets[i], typename zm::HashSet<Key>::Bucket{ZMEYA_HASH_CHAIN_NIL});
    }

    constexpr size_t alignOfNode = std::alignment_of<typename zm::HashSet<Key>::Node>::value;
    constexpr size_t sizeOfNode = sizeof(typename zm::HashSet<Key>::Node);
    zm::goffset_t nodesDataOffset = alloc_aligned(sizeOfNode * numElements, alignOfNode);
    buckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
    typename zm::HashSet<Key>::Node* nodes = reinterpret_cast<typename zm::HashSet<Key>::Node*>(get_ptr_unsafe_to_store(nodesDataOffset));

    // Re-resolve buckets/nodes from offsets after each growth; do not hold a Node& across String assign.

    size_t idx = 0;
    for (const auto& item : from)
    {
        size_t h;
        if constexpr (std::is_same_v<F, std::string>)
        {
            h = HashUtils::hashString(item.c_str());
        }
        else
        {
            h = HashUtils::hasher(item);
        }
        const size_t bucketIndex = h % hashMod;
        BuilderBase::placementCtor<Key>(&nodes[idx].key);
        nodes[idx].key = item;
        buckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
        nodes = reinterpret_cast<typename zm::HashSet<Key>::Node*>(get_ptr_unsafe_to_store(nodesDataOffset));
        nodes[idx].next = buckets[bucketIndex].head;
        buckets[bucketIndex].head = uint32_t(idx);
        ++idx;
    }

    to = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(to_offset));
    to->live_count_ = uint32_t(numElements);
    to->free_head_ = ZMEYA_HASH_CHAIN_NIL;
    to->buckets.numElements = uint32_t(numBuckets);
    to->buckets.relativeOffset = get_relative_offset(&to->buckets, bucketsDataOffset);
    to->nodes.numElements = uint32_t(numElements);
    to->nodes.relativeOffset = get_relative_offset(&to->nodes, nodesDataOffset);
    register_roffset_slot(get_global_offset(&to->buckets), bucketsDataOffset);
    register_roffset_slot(get_global_offset(&to->nodes), nodesDataOffset);
}

template <typename Key, typename Value, typename FK, typename FV>
void BuilderBase::impl_assign_hashmap(zm::HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from)
{
    ScopedBuilder tls(this);
    goffset_t to_offset = get_global_offset(&_to);
    zm::HashMap<Key, Value>* to = reinterpret_cast<zm::HashMap<Key, Value>*>(get_ptr_unsafe_to_store(to_offset));
    note_dead_hashmap_if_any<Key, Value>(to);
    if (from.empty())
    {
        to->live_count_ = 0;
        to->free_head_ = ZMEYA_HASH_CHAIN_NIL;
        to->buckets.numElements = 0;
        to->buckets.relativeOffset = 0;
        to->nodes.numElements = 0;
        to->nodes.relativeOffset = 0;
        register_roffset_slot(get_global_offset(&to->buckets), goffset_t(0));
        register_roffset_slot(get_global_offset(&to->nodes), goffset_t(0));
        return;
    }

    const size_t numElements = from.size();
    size_t numBuckets = numElements * 2;
    ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
    if (numBuckets == 0)
    {
        numBuckets = 1;
    }
    const size_t hashMod = numBuckets;

    constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashMap<Key, Value>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename zm::HashMap<Key, Value>::Bucket);
    zm::goffset_t bucketsDataOffset = alloc_aligned(sizeOfBucket * numBuckets, alignOfBucket);

    typename zm::HashMap<Key, Value>::Bucket* buckets = reinterpret_cast<typename zm::HashMap<Key, Value>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
    for (size_t i = 0; i < numBuckets; ++i)
    {
        BuilderBase::placementCtor<typename zm::HashMap<Key, Value>::Bucket>(&buckets[i], typename zm::HashMap<Key, Value>::Bucket{ZMEYA_HASH_CHAIN_NIL});
    }

    using NodeType = typename zm::HashMap<Key, Value>::Node;
    constexpr size_t alignOfNode = std::alignment_of<NodeType>::value;
    constexpr size_t sizeOfNode = sizeof(NodeType);
    zm::goffset_t nodesDataOffset = alloc_aligned(sizeOfNode * numElements, alignOfNode);
    buckets = reinterpret_cast<typename zm::HashMap<Key, Value>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
    NodeType* nodes = reinterpret_cast<NodeType*>(get_ptr_unsafe_to_store(nodesDataOffset));

    // Re-resolve buckets/nodes from offsets after each growth; do not hold a Node& across key/value assigns.

    size_t idx = 0;
    for (const auto& kv : from)
    {
        const FK& key = kv.first;
        const FV& value = kv.second;
        size_t h;
        if constexpr (std::is_same_v<FK, std::string>)
        {
            h = HashUtils::hashString(key.c_str());
        }
        else
        {
            h = HashUtils::hasher(key);
        }
        const size_t bucketIndex = h % hashMod;
        BuilderBase::placementCtor<Key>(&nodes[idx].key);
        nodes[idx].key = key;
        buckets = reinterpret_cast<typename zm::HashMap<Key, Value>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
        nodes = reinterpret_cast<NodeType*>(get_ptr_unsafe_to_store(nodesDataOffset));
        BuilderBase::placementCtor<Value>(&nodes[idx].value);
        nodes[idx].value = value;
        buckets = reinterpret_cast<typename zm::HashMap<Key, Value>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
        nodes = reinterpret_cast<NodeType*>(get_ptr_unsafe_to_store(nodesDataOffset));
        nodes[idx].next = buckets[bucketIndex].head;
        buckets[bucketIndex].head = uint32_t(idx);
        ++idx;
    }

    to = reinterpret_cast<zm::HashMap<Key, Value>*>(get_ptr_unsafe_to_store(to_offset));
    to->live_count_ = uint32_t(numElements);
    to->free_head_ = ZMEYA_HASH_CHAIN_NIL;
    to->buckets.numElements = uint32_t(numBuckets);
    to->buckets.relativeOffset = get_relative_offset(&to->buckets, bucketsDataOffset);
    to->nodes.numElements = uint32_t(numElements);
    to->nodes.relativeOffset = get_relative_offset(&to->nodes, nodesDataOffset);
    register_roffset_slot(get_global_offset(&to->buckets), bucketsDataOffset);
    register_roffset_slot(get_global_offset(&to->nodes), nodesDataOffset);
}
