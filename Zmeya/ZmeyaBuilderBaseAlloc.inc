    goffset_t alloc_aligned(size_t numBytes, size_t alignment)
    {
        ZMEYA_ASSERT(isPowerOfTwo(alignment));
        ZMEYA_ASSERT(alignment <= ZMEYA_MAX_ALIGN);

        size_t cursor = data.size();

        // Calculate padding for alignment
        size_t off = cursor & (alignment - 1);
        size_t padding = (off != 0) ? (alignment - off) : 0;
        ZMEYA_ASSERT(padding <= SIZE_MAX - cursor);
        size_t allocOffset = cursor + padding;
        ZMEYA_ASSERT(numBytes <= SIZE_MAX - allocOffset);
        size_t totalBytes = numBytes + padding;
        ZMEYA_ASSERT(totalBytes >= numBytes);

        // Resize with zero-initialization
        ZMEYA_ASSERT(cursor <= SIZE_MAX - totalBytes);
#if defined(_DEBUG) || defined(ZMEYA_DEBUG_ARENA_EPOCH)
        const size_t cap_before = data.capacity();
#endif
        data.resize(cursor + totalBytes, char(0));
#if defined(_DEBUG) || defined(ZMEYA_DEBUG_ARENA_EPOCH)
        if (data.capacity() != cap_before)
        {
            ++arena_realloc_epoch_;
        }
#endif

        // Verify alignment
        ZMEYA_ASSERT((uintptr_t(&data[allocOffset]) & (alignment - 1)) == 0);

        // check that global offset fit
        ZMEYA_ASSERT((allocOffset >= 0 && allocOffset < std::numeric_limits<goffset_t>::max()) && "Offset is too big, more that 2GB?");
        return goffset_t(allocOffset);
    }

    template <typename T, typename... _Valty> static void placementCtor(void* ptr, _Valty&&... _Val)
    {
        ::new (const_cast<void*>(static_cast<const volatile void*>(ptr))) T(std::forward<_Valty>(_Val)...);
    }

    void* get_ptr_unsafe_to_store(goffset_t g_offs)
    {
        ZMEYA_ASSERT(!data.empty());
        ZMEYA_ASSERT(g_offs >= goffset_t(0));
        ZMEYA_ASSERT(size_t(g_offs) < data.size());
        return &data[size_t(g_offs)];
    }

    template <typename T> T* allocate()
    {
        static_assert(std::is_trivially_copyable<T>::value, "Only trivially copyable types allowed");
        goffset_t g_offs = alloc_aligned(sizeof(T), alignof(T));
        void* ptr = get_ptr_unsafe_to_store(g_offs);
        BuilderBase::placementCtor<T>(ptr);
        return reinterpret_cast<T*>(ptr);
    }

    template <typename T> void note_dead_array_slab_if_any(Array<T>* arr_hdr)
    {
        if (arr_hdr->numElements == 0 && arr_hdr->relativeOffset == 0)
        {
            return;
        }
        const size_t n = size_t(arr_hdr->numElements);
        ZMEYA_ASSERT(n == 0 || sizeof(T) <= SIZE_MAX / n);
        T* pdata = arr_hdr->get_raw_ptr_unsafe_can_be_relocated();
        note_dead_range(arena_byte_offset_of(pdata), n * sizeof(T));
    }

    template <typename Key> void note_dead_hashset_if_any(HashSet<Key>* hs)
    {
        note_dead_array_slab_if_any<typename HashSet<Key>::Bucket>(&hs->buckets);
        note_dead_array_slab_if_any<typename HashSet<Key>::Node>(&hs->nodes);
    }

    template <typename K, typename V> void note_dead_hashmap_if_any(HashMap<K, V>* hm)
    {
        note_dead_array_slab_if_any<typename HashMap<K, V>::Bucket>(&hm->buckets);
        using NodeType = typename HashMap<K, V>::Node;
        note_dead_array_slab_if_any<NodeType>(&hm->nodes);
    }

    // Helper methods for assignment functions
    bool contains_pointer(const void* ptr) const
    {
        if (data.empty())
        {
            return false;
        }
        const char* dataStart = data.data();
        const char* dataEnd = dataStart + data.size();
        return (ptr >= dataStart && ptr < dataEnd);
    }

#if defined(_DEBUG) || defined(ZMEYA_DEBUG_ARENA_EPOCH)
    uint64_t arena_realloc_epoch() const noexcept { return arena_realloc_epoch_; }
#endif

    static size_t hash_assign_bucket_count_(size_t numElements)
    {
        size_t numBuckets = numElements * 2;
        ZMEYA_ASSERT(numBuckets < size_t(std::numeric_limits<uint32_t>::max()));
        if (numBuckets == 0)
        {
            numBuckets = 1;
        }
        return numBuckets;
    }

    template <typename Bucket> goffset_t alloc_hash_bucket_slab_zeroed_heads_(size_t numBuckets);
