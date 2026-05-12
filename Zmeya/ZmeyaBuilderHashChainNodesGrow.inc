namespace detail
{

template <typename Key> void BuilderBase::hash_chain_nodes_array_grow_append_default_hashset(zm::Array<typename zm::HashSet<Key>::Node>& arr)
{
    using Node = typename zm::HashSet<Key>::Node;
    goffset_t hdr_g = arena_byte_offset_of(&arr);
    zm::Array<Node>* slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    size_t old_n = size_t(slot->numElements);
    size_t new_n = old_n + 1;
    constexpr size_t alignOfNode = std::alignment_of<Node>::value;
    constexpr size_t sizeOfNode = sizeof(Node);

    size_t old_cap = old_n;
    const auto itPhys = hash_chain_nodes_phys_bytes_.find(hdr_g);
    if (itPhys != hash_chain_nodes_phys_bytes_.end())
    {
        ZMEYA_ASSERT(itPhys->second % sizeOfNode == 0);
        old_cap = itPhys->second / sizeOfNode;
    }

    if (old_cap > 0 && new_n <= old_cap)
    {
        slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
        Node* base = slot->get_raw_ptr_unsafe_can_be_relocated();
        Node* new_tail = base + old_n;
        BuilderBase::placementCtor<Key>(&new_tail->key);
        new_tail->next = ZMEYA_HASH_CHAIN_NIL;
        slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
        slot->numElements = uint32_t(new_n);
        return;
    }

    size_t new_cap = new_n;
    if (old_n == 0)
    {
        new_cap = (std::max)(size_t(8), new_n);
        const auto hintIt = hash_chain_nodes_first_alloc_hint_.find(hdr_g);
        if (hintIt != hash_chain_nodes_first_alloc_hint_.end())
        {
            new_cap = (std::max)(new_cap, hintIt->second);
            hash_chain_nodes_first_alloc_hint_.erase(hintIt);
        }
    }
    else
    {
        ZMEYA_ASSERT(old_n <= SIZE_MAX / 2);
        new_cap = (std::max)(new_n, old_cap * 2);
    }
    ZMEYA_ASSERT(new_cap <= SIZE_MAX / sizeOfNode);

    size_t dead_slab_bytes = old_n * sizeOfNode;
    if (itPhys != hash_chain_nodes_phys_bytes_.end())
    {
        dead_slab_bytes = itPhys->second;
    }
    if (old_n > 0)
    {
        Node* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
        note_dead_range(arena_byte_offset_of(old_data), dead_slab_bytes);
    }
    goffset_t new_blob_g = alloc_aligned(sizeOfNode * new_cap, alignOfNode);
    slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    if (old_n > 0)
    {
        for (size_t i = 0; i < old_n; ++i)
        {
            slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
            Node* old_row = slot->get_raw_ptr_unsafe_can_be_relocated();
            const uint32_t saved_next = old_row[i].next;

            std::string old_key_copy;
            Key old_key_copy_trivial{};
            if constexpr (std::is_same<Key, zm::String>::value)
            {
                old_key_copy.assign(old_row[i].key.c_str());
            }
            else
            {
                old_key_copy_trivial = old_row[i].key;
            }

            Node* new_row = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(i * sizeOfNode)));
            BuilderBase::placementCtor<Key>(&new_row->key);
            if constexpr (std::is_same<Key, zm::String>::value)
            {
                assign_string_std(*this, new_row->key, old_key_copy);
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
    }
    Node* new_tail = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(old_n * sizeOfNode)));
    BuilderBase::placementCtor<Key>(&new_tail->key);
    new_tail->next = ZMEYA_HASH_CHAIN_NIL;
    slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    slot->numElements = uint32_t(new_n);
    slot->relativeOffset = get_relative_offset(slot, new_blob_g);
    register_roffset_slot(hdr_g, new_blob_g);
    hash_chain_nodes_phys_bytes_[hdr_g] = new_cap * sizeOfNode;
}

template <typename K, typename V> void BuilderBase::hash_chain_nodes_array_grow_append_default_hashmap(zm::Array<typename zm::HashMap<K, V>::Node>& arr)
{
    using Node = typename zm::HashMap<K, V>::Node;
    goffset_t hdr_g = arena_byte_offset_of(&arr);
    zm::Array<Node>* slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    size_t old_n = size_t(slot->numElements);
    size_t new_n = old_n + 1;
    constexpr size_t alignOfNode = std::alignment_of<Node>::value;
    constexpr size_t sizeOfNode = sizeof(Node);

    size_t old_cap = old_n;
    const auto itPhys = hash_chain_nodes_phys_bytes_.find(hdr_g);
    if (itPhys != hash_chain_nodes_phys_bytes_.end())
    {
        ZMEYA_ASSERT(itPhys->second % sizeOfNode == 0);
        old_cap = itPhys->second / sizeOfNode;
    }

    if (old_cap > 0 && new_n <= old_cap)
    {
        slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
        Node* base = slot->get_raw_ptr_unsafe_can_be_relocated();
        Node* new_tail = base + old_n;
        BuilderBase::placementCtor<K>(&new_tail->key);
        BuilderBase::placementCtor<V>(&new_tail->value);
        new_tail->next = ZMEYA_HASH_CHAIN_NIL;
        slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
        slot->numElements = uint32_t(new_n);
        return;
    }

    size_t new_cap = new_n;
    if (old_n == 0)
    {
        new_cap = (std::max)(size_t(8), new_n);
        const auto hintIt = hash_chain_nodes_first_alloc_hint_.find(hdr_g);
        if (hintIt != hash_chain_nodes_first_alloc_hint_.end())
        {
            new_cap = (std::max)(new_cap, hintIt->second);
            hash_chain_nodes_first_alloc_hint_.erase(hintIt);
        }
    }
    else
    {
        ZMEYA_ASSERT(old_n <= SIZE_MAX / 2);
        new_cap = (std::max)(new_n, old_cap * 2);
    }
    ZMEYA_ASSERT(new_cap <= SIZE_MAX / sizeOfNode);

    size_t dead_slab_bytes = old_n * sizeOfNode;
    if (itPhys != hash_chain_nodes_phys_bytes_.end())
    {
        dead_slab_bytes = itPhys->second;
    }
    if (old_n > 0)
    {
        Node* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
        note_dead_range(arena_byte_offset_of(old_data), dead_slab_bytes);
    }
    goffset_t new_blob_g = alloc_aligned(sizeOfNode * new_cap, alignOfNode);
    slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    if (old_n > 0)
    {
        for (size_t i = 0; i < old_n; ++i)
        {
            slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
            Node* old_row = slot->get_raw_ptr_unsafe_can_be_relocated();
            const uint32_t saved_next = old_row[i].next;

            std::string old_key_copy;
            std::string old_val_copy;
            K old_key_copy_trivial{};
            V old_val_copy_trivial{};
            if constexpr (std::is_same<K, zm::String>::value)
            {
                old_key_copy.assign(old_row[i].key.c_str());
            }
            else
            {
                old_key_copy_trivial = old_row[i].key;
            }
            if constexpr (std::is_same<V, zm::String>::value)
            {
                old_val_copy.assign(old_row[i].value.c_str());
            }
            else
            {
                old_val_copy_trivial = old_row[i].value;
            }

            Node* new_row = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(i * sizeOfNode)));
            BuilderBase::placementCtor<K>(&new_row->key);
            BuilderBase::placementCtor<V>(&new_row->value);
            if constexpr (std::is_same<K, zm::String>::value)
            {
                assign_string_std(*this, new_row->key, old_key_copy);
                new_row = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(i * sizeOfNode)));
            }
            else
            {
                new_row->key = old_key_copy_trivial;
            }
            if constexpr (std::is_same<V, zm::String>::value)
            {
                assign_string_std(*this, new_row->value, old_val_copy);
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
    }
    Node* new_tail = reinterpret_cast<Node*>(get_ptr_unsafe_to_store(new_blob_g + goffset_t(old_n * sizeOfNode)));
    BuilderBase::placementCtor<K>(&new_tail->key);
    BuilderBase::placementCtor<V>(&new_tail->value);
    new_tail->next = ZMEYA_HASH_CHAIN_NIL;
    slot = reinterpret_cast<zm::Array<Node>*>(get_ptr_unsafe_to_store(hdr_g));
    slot->numElements = uint32_t(new_n);
    slot->relativeOffset = get_relative_offset(slot, new_blob_g);
    register_roffset_slot(hdr_g, new_blob_g);
    hash_chain_nodes_phys_bytes_[hdr_g] = new_cap * sizeOfNode;
}

} // namespace detail
