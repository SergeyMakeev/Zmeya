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
