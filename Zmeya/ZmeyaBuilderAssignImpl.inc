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
        to->buckets.numElements = 0;
        to->buckets.relativeOffset = 0;
        to->items.numElements = 0;
        to->items.relativeOffset = 0;
        register_roffset_slot(get_global_offset(&to->buckets), goffset_t(0));
        register_roffset_slot(get_global_offset(&to->items), goffset_t(0));
        return;
    }

    size_t numElements = from.size();
    size_t numBuckets = numElements * 2;
    ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
    size_t hashMod = numBuckets;

    constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashSet<Key>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename zm::HashSet<Key>::Bucket);
    zm::goffset_t bucketsDataOffset = alloc_aligned(sizeOfBucket * numBuckets, alignOfBucket);

    typename zm::HashSet<Key>::Bucket* buckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
    for (size_t i = 0; i < numBuckets; ++i)
    {
        BuilderBase::placementCtor<typename zm::HashSet<Key>::Bucket>(&buckets[i], typename zm::HashSet<Key>::Bucket{0, 0});
    }

    std::vector<size_t> item_hashes;
    item_hashes.reserve(from.size());
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
        item_hashes.push_back(h);
    }
    for (size_t hi = 0; hi < item_hashes.size(); ++hi)
    {
        size_t bucketIndex = item_hashes[hi] % hashMod;
        buckets[bucketIndex].beginIndex++;
    }

    size_t beginIndex = 0;
    for (size_t bucketIndex = 0; bucketIndex < numBuckets; bucketIndex++)
    {
        typename zm::HashSet<Key>::Bucket& bucket = buckets[bucketIndex];
        size_t numElementsInBucket = bucket.beginIndex;
        bucket.beginIndex = uint32_t(beginIndex);
        bucket.endIndex = bucket.beginIndex;
        beginIndex += numElementsInBucket;
    }

    constexpr size_t alignOfKey = std::alignment_of<Key>::value;
    constexpr size_t sizeOfKey = sizeof(Key);
    zm::goffset_t itemsDataOffset = alloc_aligned(sizeOfKey * numElements, alignOfKey);

    Key* items = reinterpret_cast<Key*>(get_ptr_unsafe_to_store(itemsDataOffset));
    buckets = reinterpret_cast<typename zm::HashSet<Key>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));

    size_t hi = 0;
    for (const auto& item : from)
    {
        size_t bucketIndex = item_hashes[hi++] % hashMod;
        typename zm::HashSet<Key>::Bucket& bucket = buckets[bucketIndex];

        Key* element = &items[bucket.endIndex];
        BuilderBase::placementCtor<Key>(element);

        *element = item;

        bucket.endIndex++;
    }

    to = reinterpret_cast<zm::HashSet<Key>*>(get_ptr_unsafe_to_store(to_offset));
    to->buckets.numElements = uint32_t(numBuckets);
    to->buckets.relativeOffset = get_relative_offset(&to->buckets, bucketsDataOffset);
    to->items.numElements = uint32_t(numElements);
    to->items.relativeOffset = get_relative_offset(&to->items, itemsDataOffset);
    register_roffset_slot(get_global_offset(&to->buckets), bucketsDataOffset);
    register_roffset_slot(get_global_offset(&to->items), itemsDataOffset);
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
        to->buckets.numElements = 0;
        to->buckets.relativeOffset = 0;
        to->items.numElements = 0;
        to->items.relativeOffset = 0;
        register_roffset_slot(get_global_offset(&to->buckets), goffset_t(0));
        register_roffset_slot(get_global_offset(&to->items), goffset_t(0));
        return;
    }

    size_t numElements = from.size();
    size_t numBuckets = numElements * 2;
    ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
    size_t hashMod = numBuckets;

    constexpr size_t alignOfBucket = std::alignment_of<typename zm::HashMap<Key, Value>::Bucket>::value;
    constexpr size_t sizeOfBucket = sizeof(typename zm::HashMap<Key, Value>::Bucket);
    zm::goffset_t bucketsDataOffset = alloc_aligned(sizeOfBucket * numBuckets, alignOfBucket);

    typename zm::HashMap<Key, Value>::Bucket* buckets = reinterpret_cast<typename zm::HashMap<Key, Value>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));
    for (size_t i = 0; i < numBuckets; ++i)
    {
        BuilderBase::placementCtor<typename zm::HashMap<Key, Value>::Bucket>(&buckets[i], typename zm::HashMap<Key, Value>::Bucket{0, 0});
    }

    std::vector<size_t> key_hashes;
    key_hashes.reserve(from.size());
    for (const auto& kv : from)
    {
        const FK& key = kv.first;
        size_t h;
        if constexpr (std::is_same_v<FK, std::string>)
        {
            h = HashUtils::hashString(key.c_str());
        }
        else
        {
            h = HashUtils::hasher(key);
        }
        key_hashes.push_back(h);
    }
    size_t khi = 0;
    for (const auto& kv : from)
    {
        (void)kv;
        size_t bucketIndex = key_hashes[khi++] % hashMod;
        buckets[bucketIndex].beginIndex++;
    }

    size_t beginIndex = 0;
    for (size_t bucketIndex = 0; bucketIndex < numBuckets; bucketIndex++)
    {
        typename zm::HashMap<Key, Value>::Bucket& bucket = buckets[bucketIndex];
        size_t numElementsInBucket = bucket.beginIndex;
        bucket.beginIndex = uint32_t(beginIndex);
        bucket.endIndex = bucket.beginIndex;
        beginIndex += numElementsInBucket;
    }

    using ItemType = zm::Pair<const Key, Value>;
    constexpr size_t alignOfItem = std::alignment_of<ItemType>::value;
    constexpr size_t sizeOfItem = sizeof(ItemType);
    zm::goffset_t itemsDataOffset = alloc_aligned(sizeOfItem * numElements, alignOfItem);

    ItemType* items = reinterpret_cast<ItemType*>(get_ptr_unsafe_to_store(itemsDataOffset));
    buckets = reinterpret_cast<typename zm::HashMap<Key, Value>::Bucket*>(get_ptr_unsafe_to_store(bucketsDataOffset));

    khi = 0;
    for (const auto& kv : from)
    {
        const FK& key = kv.first;
        const FV& value = kv.second;
        size_t bucketIndex = key_hashes[khi++] % hashMod;
        typename zm::HashMap<Key, Value>::Bucket& bucket = buckets[bucketIndex];

        ItemType* element = &items[bucket.endIndex];

        Key* mutableKey = const_cast<Key*>(&element->first);
        BuilderBase::placementCtor<Key>(mutableKey);
        BuilderBase::placementCtor<Value>(&element->second);

        *mutableKey = key;
        element->second = value;

        bucket.endIndex++;
    }

    to = reinterpret_cast<zm::HashMap<Key, Value>*>(get_ptr_unsafe_to_store(to_offset));
    to->buckets.numElements = uint32_t(numBuckets);
    to->buckets.relativeOffset = get_relative_offset(&to->buckets, bucketsDataOffset);
    to->items.numElements = uint32_t(numElements);
    to->items.relativeOffset = get_relative_offset(&to->items, itemsDataOffset);
    register_roffset_slot(get_global_offset(&to->buckets), bucketsDataOffset);
    register_roffset_slot(get_global_offset(&to->items), itemsDataOffset);
}
