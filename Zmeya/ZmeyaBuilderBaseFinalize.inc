    template <typename T> void impl_assign_pointer_array(zm::Array<zm::Pointer<T>>& _to, const std::vector<T*>& from);

    template <typename T, typename F> void impl_assign_array_vector(zm::Array<T>& _to, const std::vector<F>& from);

    template <typename Key, typename F> void impl_assign_hashset(zm::HashSet<Key>& _to, const std::unordered_set<F>& from);

    template <typename Key, typename Value, typename FK, typename FV>
    void impl_assign_hashmap(zm::HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from);

    /*
    **finalize**

    Phase 0: compact bump arena and remap roffset registry targets.
    Phase 1: grow padding to alignment (layout bytes in-place).
    Phase 2: patch every registered self-relative word from parallel goffset targets (Q9).
    */

    void finalize_in_place(size_t alignment)
    {
        ZMEYA_ASSERT(alignment > 0);
        ZMEYA_ASSERT(isPowerOfTwo(alignment));
        compact_arena_and_remap_registry();
        ZMEYA_ASSERT(dead_ranges_.empty());

        size_t currentSize = data.size();
        size_t remainder = currentSize % alignment;
        if (remainder != 0)
        {
            size_t paddingNeeded = alignment - remainder;
            ZMEYA_ASSERT(paddingNeeded <= SIZE_MAX - currentSize);
            data.resize(currentSize + paddingNeeded, char(0));
        }

        ZMEYA_ASSERT((data.size() % alignment) == 0);

        patch_roffset_slots_from_registry();
    }

    Span<char> finalize(size_t alignment = 4)
    {
        finalize_in_place(alignment);
        return Span<char>(data.data(), data.size());
    }

    BlobBuffer finalize_move_out(size_t alignment = 4)
    {
        finalize_in_place(alignment);
        BlobBuffer out;
        out.swap(data);
        return out;
    }

    template <typename T> roffset_t get_relative_offset(const T* base, goffset_t ofs) const
    {
        ZMEYA_ASSERT(is_stack_pointer(base) == false && "Stack pointer detected!");
        ZMEYA_ASSERT(contains_pointer(base) && "A pointer should belong to the builder");
        ZMEYA_ASSERT(ofs >= goffset_t(0));
        ZMEYA_ASSERT(size_t(ofs) < data.size());

        goffset_t baseOffset = arena_byte_offset_of(base);
        const diff_t byte_delta = diff_t(ofs) - diff_t(baseOffset);

        ZMEYA_ASSERT(byte_delta >= diff_t(std::numeric_limits<roffset_t>::min()));
        ZMEYA_ASSERT(byte_delta <= diff_t(std::numeric_limits<roffset_t>::max()));
        return roffset_t(byte_delta);
    }

    template <typename Key, typename F> void hashset_chain_insert(zm::HashSet<Key>& hs, const F& item);

    template <typename Key, typename F> void hashset_chain_erase(zm::HashSet<Key>& hs, const F& key);

    template <typename Key> void hashset_chain_clear(zm::HashSet<Key>& hs);

    template <typename Key> void hashset_rehash_impl(zm::HashSet<Key>& hs, size_t new_num_buckets);

    template <typename K, typename V, typename FK, typename FV> void hashmap_chain_insert(zm::HashMap<K, V>& hm, const FK& key, const FV& val);

    template <typename K, typename V, typename FK> void hashmap_chain_erase(zm::HashMap<K, V>& hm, const FK& key);

    template <typename K, typename V> void hashmap_chain_clear(zm::HashMap<K, V>& hm);

    template <typename K, typename V> void hashmap_rehash_impl(zm::HashMap<K, V>& hm, size_t new_num_buckets);
