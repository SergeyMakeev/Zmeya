/*

**`detail::BuilderBase` implementation body**

Pulled into a `.inc` file so the public wrapper (`ZmeyaBuilderBase.h`) stays small. This holds arena
allocation, dead-range compaction, string/array/hash mutation helpers, and `finalize`.

*/

class BuilderBase
{
  public:
    std::vector<char, BufferAllocator<char, ZMEYA_MAX_ALIGN>> data;

    struct PrivateToken
    {
    };

    std::unordered_map<goffset_t, goffset_t> roffset_slot_targets_;

    std::unordered_map<goffset_t, size_t> string_blob_phys_bytes_;

    std::vector<std::pair<goffset_t, size_t>> dead_ranges_;

    void note_dead_range(goffset_t start, size_t len)
    {
        if (len == 0 || data.empty())
        {
            return;
        }
        ZMEYA_ASSERT(start >= goffset_t(0));
        ZMEYA_ASSERT(size_t(start) < data.size());
        ZMEYA_ASSERT(len <= data.size());
        ZMEYA_ASSERT(size_t(start) <= data.size() - len);
        dead_ranges_.push_back(std::pair<goffset_t, size_t>(start, len));
    }

    static bool goffset_in_any_merged_dead_range(goffset_t g, const std::vector<std::pair<goffset_t, size_t>>& merged)
    {
        if (g == goffset_t(0))
        {
            return false;
        }
        for (const auto& d : merged)
        {
            const goffset_t dEnd = d.first + goffset_t(d.second);
            if (g >= d.first && g < dEnd)
            {
                return true;
            }
        }
        return false;
    }

    void prune_roffset_registry_overlapping_merged_dead(const std::vector<std::pair<goffset_t, size_t>>& merged)
    {
        for (auto it = roffset_slot_targets_.begin(); it != roffset_slot_targets_.end();)
        {
            const goffset_t sg = it->first;
            const goffset_t tg = it->second;
            if (goffset_in_any_merged_dead_range(sg, merged) || goffset_in_any_merged_dead_range(tg, merged))
            {
                it = roffset_slot_targets_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    static void merge_intervals(std::vector<std::pair<goffset_t, size_t>>& iv)
    {
        if (iv.empty())
        {
            return;
        }
        std::sort(iv.begin(), iv.end(), [](const std::pair<goffset_t, size_t>& a, const std::pair<goffset_t, size_t>& b) { return a.first < b.first; });
        std::vector<std::pair<goffset_t, size_t>> out;
        out.reserve(iv.size());
        goffset_t curS = iv[0].first;
        size_t curE = iv[0].second;
        for (size_t i = 1; i < iv.size(); ++i)
        {
            goffset_t s = iv[i].first;
            size_t len = iv[i].second;
            goffset_t curEnd = curS + goffset_t(curE);
            if (s <= curEnd)
            {
                goffset_t newEnd = (std::max)(curEnd, s + goffset_t(len));
                curE = size_t(newEnd - curS);
            }
            else
            {
                out.push_back(std::pair<goffset_t, size_t>(curS, curE));
                curS = s;
                curE = len;
            }
        }
        out.push_back(std::pair<goffset_t, size_t>(curS, curE));
        iv.swap(out);
    }

#if defined(ZMEYA_BUILDER_PARANOID)
    void debug_validate_roffset_registry_before_patch() const
    {
        const size_t n = data.size();
        for (const auto& kv : roffset_slot_targets_)
        {
            ZMEYA_ASSERT(kv.first >= goffset_t(0));
            ZMEYA_ASSERT(size_t(kv.first) + sizeof(roffset_t) <= n);
            if (kv.second != goffset_t(0))
            {
                ZMEYA_ASSERT(kv.second >= goffset_t(0));
                ZMEYA_ASSERT(size_t(kv.second) < n);
            }
        }
    }
#endif

    void compact_arena_and_remap_registry()
    {
        if (dead_ranges_.empty())
        {
            return;
        }

        merge_intervals(dead_ranges_);
        prune_roffset_registry_overlapping_merged_dead(dead_ranges_);

        const size_t oldSize = data.size();
        goffset_t prevDeadEnd = goffset_t(0);
        for (const auto& d : dead_ranges_)
        {
            ZMEYA_ASSERT(d.first >= goffset_t(0));
            ZMEYA_ASSERT(d.second > 0);
            ZMEYA_ASSERT(size_t(d.first) < oldSize);
            ZMEYA_ASSERT(d.second <= oldSize);
            ZMEYA_ASSERT(size_t(d.first) <= oldSize - d.second);
            ZMEYA_ASSERT(d.first >= prevDeadEnd);
            prevDeadEnd = d.first + goffset_t(d.second);
            ZMEYA_ASSERT(prevDeadEnd <= goffset_t(oldSize));
        }

        goffset_t scan = 0;
        std::vector<char, BufferAllocator<char, ZMEYA_MAX_ALIGN>> newData;
        struct Piece
        {
            goffset_t old_lo;
            goffset_t old_hi;
            goffset_t new_lo;
        };
        std::vector<Piece> pieces;
        pieces.reserve(dead_ranges_.size() + 1);

        goffset_t newCursor = 0;
        for (const auto& d : dead_ranges_)
        {
            goffset_t dStart = d.first;
            size_t dLen = d.second;
            if (scan < dStart)
            {
                size_t runLen = size_t(dStart - scan);
                newData.resize(newData.size() + runLen);
                std::memcpy(newData.data() + size_t(newCursor), data.data() + size_t(scan), runLen);
                pieces.push_back(Piece{scan, dStart, newCursor});
                newCursor += goffset_t(runLen);
            }
            scan = dStart + goffset_t(dLen);
        }
        if (scan < goffset_t(oldSize))
        {
            size_t runLen = size_t(goffset_t(oldSize) - scan);
            newData.resize(newData.size() + runLen);
            std::memcpy(newData.data() + size_t(newCursor), data.data() + size_t(scan), runLen);
            pieces.push_back(Piece{scan, goffset_t(oldSize), newCursor});
            newCursor += goffset_t(runLen);
            scan = goffset_t(oldSize);
        }
        ZMEYA_ASSERT(scan == goffset_t(oldSize));
        ZMEYA_ASSERT(newData.size() <= oldSize);
        ZMEYA_ASSERT(size_t(newCursor) == newData.size());

        auto remap_one = [&](goffset_t oldG) -> goffset_t {
            ZMEYA_ASSERT(!pieces.empty());
            auto it = std::upper_bound(pieces.begin(), pieces.end(), oldG, [](goffset_t g, const Piece& p) { return g < p.old_lo; });
            ZMEYA_ASSERT(it != pieces.begin());
            --it;
            ZMEYA_ASSERT(oldG >= it->old_lo && oldG < it->old_hi);
            return it->new_lo + (oldG - it->old_lo);
        };

        std::unordered_map<goffset_t, goffset_t> newMap;
        newMap.reserve(roffset_slot_targets_.size() + 8);
        for (const auto& kv : roffset_slot_targets_)
        {
            goffset_t nk = remap_one(kv.first);
            goffset_t nt = (kv.second == goffset_t(0)) ? goffset_t(0) : remap_one(kv.second);
            newMap[nk] = nt;
        }
        roffset_slot_targets_ = std::move(newMap);

        data.swap(newData);
        dead_ranges_.clear();
        string_blob_phys_bytes_.clear();
    }

    void register_roffset_slot(goffset_t slot_field_goffset, goffset_t target_goffset)
    {
        ZMEYA_ASSERT(!data.empty());
        ZMEYA_ASSERT(slot_field_goffset >= goffset_t(0));
        ZMEYA_ASSERT(size_t(slot_field_goffset) + sizeof(roffset_t) <= data.size());
        if (target_goffset != goffset_t(0))
        {
            ZMEYA_ASSERT(target_goffset >= goffset_t(0));
            ZMEYA_ASSERT(size_t(target_goffset) < data.size());
        }
        roffset_slot_targets_[slot_field_goffset] = target_goffset;
    }

    void string_blob_phys_register(goffset_t blob_start, size_t nbytes)
    {
        string_blob_phys_bytes_[blob_start] = nbytes;
    }

    void patch_roffset_slots_from_registry()
    {
#if defined(ZMEYA_BUILDER_PARANOID)
        debug_validate_roffset_registry_before_patch();
#endif
        for (const auto& kv : roffset_slot_targets_)
        {
            goffset_t slot_g = kv.first;
            goffset_t target_g = kv.second;
            ZMEYA_ASSERT(size_t(slot_g) + sizeof(roffset_t) <= data.size());
            roffset_t* pr = reinterpret_cast<roffset_t*>(&data[slot_g]);
            if (target_g == 0)
            {
                *pr = 0;
            }
            else
            {
                const void* slotPtr = &data[slot_g];
                goffset_t baseG = get_global_offset(slotPtr);
                diff_t d = diff_t(goffset_t(target_g)) - diff_t(goffset_t(baseG));
                ZMEYA_ASSERT(d >= diff_t(std::numeric_limits<roffset_t>::min()));
                ZMEYA_ASSERT(d <= diff_t(std::numeric_limits<roffset_t>::max()));
                *pr = roffset_t(d);
            }
        }
    }

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
        data.resize(cursor + totalBytes, char(0));

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
        note_dead_range(get_global_offset(pdata), n * sizeof(T));
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

    // TODO: rename maybe?
    goffset_t get_global_offset(const void* ptr) const
    {
        ZMEYA_ASSERT(contains_pointer(ptr));
        offset_t allocOffset = offset_t(uintptr_t(ptr) - uintptr_t(data.data()));
        // check that global offset fit
        ZMEYA_ASSERT((allocOffset >= 0 && allocOffset < std::numeric_limits<goffset_t>::max()) && "Offset is too big, more that 2GB?");
        return goffset_t(allocOffset);
    }

    explicit BuilderBase(size_t initialSizeInBytes, PrivateToken)
    {
        // 16 bytes minimum
        size_t reserveSize = std::max(initialSizeInBytes, size_t(16));
        data.reserve(reserveSize);

        // Static assertions for zm types
        static_assert(std::is_trivially_copyable<Pointer<int>>::value, "Pointer is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<Array<int>>::value, "Array is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<HashSet<int>>::value, "HashSet is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<zm::Pair<int, float>>::value, "Pair is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<HashMap<int, int>>::value, "HashMap is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<String>::value, "String is_trivially_copyable check failed");
    }

    ~BuilderBase() = default;

    // Non-copyable, non-movable to keep it simple
    BuilderBase(const BuilderBase&) = delete;
    BuilderBase& operator=(const BuilderBase&) = delete;
    BuilderBase(BuilderBase&&) = delete;
    BuilderBase& operator=(BuilderBase&&) = delete;

    void string_note_dead_existing_blob(String& s)
    {
        goffset_t str_g = get_global_offset(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        const char* old_ptr = slot->data.get();
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            goffset_t blob_g = get_global_offset(old_ptr);
            size_t logical_len = std::strlen(old_ptr);
            size_t phys = logical_len + 1;
            auto it = string_blob_phys_bytes_.find(blob_g);
            if (it != string_blob_phys_bytes_.end())
            {
                phys = it->second;
                string_blob_phys_bytes_.erase(it);
            }
            note_dead_range(blob_g, phys);
        }
    }

    void string_append_cstr(String& s, const char* suf, size_t suf_len)
    {
        if (suf_len == 0)
        {
            return;
        }
        ZMEYA_ASSERT(suf != nullptr);
        goffset_t str_g = get_global_offset(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        size_t old_len = 0;
        goffset_t old_blob_g = goffset_t(0);
        const char* old_ptr = slot->data.get();
        size_t old_phys_for_growth = 0;
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            old_blob_g = get_global_offset(old_ptr);
            old_len = std::strlen(old_ptr);
            old_phys_for_growth = old_len + 1;
            auto pit = string_blob_phys_bytes_.find(old_blob_g);
            if (pit != string_blob_phys_bytes_.end())
            {
                old_phys_for_growth = pit->second;
                string_blob_phys_bytes_.erase(pit);
            }
            note_dead_range(old_blob_g, old_phys_for_growth);
        }
        ZMEYA_ASSERT(suf_len <= SIZE_MAX - old_len);
        ZMEYA_ASSERT(old_len + suf_len <= SIZE_MAX - 1);
        const size_t new_len = old_len + suf_len;
        size_t alloc_bytes = new_len + 1;
        if (old_ptr != nullptr)
        {
            if (old_phys_for_growth >= alloc_bytes)
            {
                alloc_bytes = old_phys_for_growth;
            }
            else
            {
                size_t cap = (std::max)(old_phys_for_growth, size_t(8));
                while (cap < alloc_bytes && cap <= SIZE_MAX / 2)
                {
                    cap *= 2;
                }
                if (cap < alloc_bytes)
                {
                    cap = alloc_bytes;
                }
                alloc_bytes = cap;
            }
        }
        goffset_t blob_g = alloc_aligned(alloc_bytes, 1);
        slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        char* dest = reinterpret_cast<char*>(get_ptr_unsafe_to_store(blob_g));
        if (old_len != 0)
        {
            const char* old_src = reinterpret_cast<const char*>(get_ptr_unsafe_to_store(old_blob_g));
            std::memcpy(dest, old_src, old_len);
        }
        std::memcpy(dest + old_len, suf, suf_len);
        dest[new_len] = '\0';
        slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        goffset_t ptr_slot_g = get_global_offset(&slot->data);
        slot->data.relativeOffset = get_relative_offset(&slot->data, blob_g);
        register_roffset_slot(ptr_slot_g, blob_g);
        string_blob_phys_bytes_[blob_g] = alloc_bytes;
    }

    void string_clear(String& s)
    {
        goffset_t str_g = get_global_offset(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        const char* old_ptr = slot->data.get();
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            goffset_t blob_g = get_global_offset(old_ptr);
            size_t phys = std::strlen(old_ptr) + 1;
            auto it = string_blob_phys_bytes_.find(blob_g);
            if (it != string_blob_phys_bytes_.end())
            {
                phys = it->second;
                string_blob_phys_bytes_.erase(it);
            }
            note_dead_range(blob_g, phys);
        }
        slot->data.relativeOffset = 0;
        register_roffset_slot(get_global_offset(&slot->data), goffset_t(0));
    }

    template <typename T, typename U> void array_push_back(Array<T>& arr, U&& value)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value,
            "array_push_back is only supported for slab-memcpy-safe element types; use assign(vector<...>) for String, Pointer, nested Array, etc.");
        goffset_t hdr_g = get_global_offset(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        size_t old_n = size_t(slot->numElements);
        size_t new_n = old_n + 1;
        constexpr size_t alignOfT = std::alignment_of<T>::value;
        constexpr size_t sizeOfT = sizeof(T);
        size_t new_cap = new_n;
        if (old_n == 0)
        {
            new_cap = (std::max)(size_t(8), new_n);
        }
        else
        {
            ZMEYA_ASSERT(old_n <= SIZE_MAX / 2);
            new_cap = (std::max)(new_n, old_n * 2);
        }
        ZMEYA_ASSERT(new_cap <= SIZE_MAX / sizeOfT);
        if (old_n > 0)
        {
            T* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
            note_dead_range(get_global_offset(old_data), old_n * sizeOfT);
        }
        goffset_t new_blob_g = alloc_aligned(sizeOfT * new_cap, alignOfT);
        slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        T* new_data = reinterpret_cast<T*>(get_ptr_unsafe_to_store(new_blob_g));
        if (old_n > 0)
        {
            T* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
            std::memcpy(new_data, old_data, old_n * sizeOfT);
        }
        placementCtor<T>(&new_data[old_n], std::forward<U>(value));
        slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        slot->numElements = uint32_t(new_n);
        slot->relativeOffset = get_relative_offset(slot, new_blob_g);
        register_roffset_slot(hdr_g, new_blob_g);
    }

    template <typename T> void array_pop_back(Array<T>& arr)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value, "array_pop_back requires same constraints as push_back for this builder");
        goffset_t hdr_g = get_global_offset(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        if (slot->numElements == 0)
        {
            return;
        }
        slot->numElements -= 1;
    }

    template <typename T> void array_clear(Array<T>& arr)
    {
        goffset_t hdr_g = get_global_offset(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        note_dead_array_slab_if_any(slot);
        slot->numElements = 0;
        slot->relativeOffset = 0;
        register_roffset_slot(hdr_g, goffset_t(0));
    }

    template <typename T> void array_erase_at(Array<T>& arr, size_t index)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value, "array_erase_at requires same constraints as push_back for this builder");
        goffset_t hdr_g = get_global_offset(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        if (index >= size_t(slot->numElements))
        {
            return;
        }
        T* pdata = slot->get_raw_ptr_unsafe_can_be_relocated();
        constexpr size_t sizeOfT = sizeof(T);
        size_t tail = size_t(slot->numElements) - index - 1;
        if (tail > 0)
        {
            std::memmove(reinterpret_cast<char*>(pdata + index), reinterpret_cast<char*>(pdata + index + 1), tail * sizeOfT);
        }
        slot->numElements -= 1;
    }

    template <typename T> void array_resize_fill(Array<T>& arr, size_t new_size, const T& fill)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value, "array_resize_fill requires same constraints as push_back for this builder");
        goffset_t hdr_g = get_global_offset(&arr);
        for (;;)
        {
            Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
            size_t old_n = size_t(slot->numElements);
            if (new_size == old_n)
            {
                return;
            }
            if (new_size < old_n)
            {
                slot->numElements = uint32_t(new_size);
                return;
            }
            array_push_back(*slot, fill);
        }
    }

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

    Span<char> finalize(size_t alignment = 4)
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

        return Span<char>(data.data(), data.size());
    }

    template <typename T> roffset_t get_relative_offset(const T* base, goffset_t ofs) const
    {
        ZMEYA_ASSERT(is_stack_pointer(base) == false && "Stack pointer detected!");
        ZMEYA_ASSERT(contains_pointer(base) && "A pointer should belong to the builder");
        ZMEYA_ASSERT(ofs >= goffset_t(0));
        ZMEYA_ASSERT(size_t(ofs) < data.size());

        goffset_t baseOffset = get_global_offset(base);
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


};