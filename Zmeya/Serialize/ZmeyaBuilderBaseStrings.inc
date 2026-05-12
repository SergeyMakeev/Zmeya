    void string_note_dead_existing_blob(String& s)
    {
        goffset_t str_g = arena_byte_offset_of(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        const char* old_ptr = slot->data.get();
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            goffset_t blob_g = arena_byte_offset_of(old_ptr);
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
        goffset_t str_g = arena_byte_offset_of(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        size_t old_len = 0;
        goffset_t old_blob_g = goffset_t(0);
        const char* old_ptr = slot->data.get();
        size_t old_phys_for_growth = 0;
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            old_blob_g = arena_byte_offset_of(old_ptr);
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
        if (old_ptr == nullptr)
        {
            // First allocation: reserve a small minimum so chatty append from an empty string does not
            // grow the arena on every tiny suffix (audit: string_append chatter).
            const size_t kMinFirstBlob = 32;
            if (alloc_bytes < kMinFirstBlob)
            {
                alloc_bytes = kMinFirstBlob;
            }
        }
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
        goffset_t ptr_slot_g = arena_byte_offset_of(&slot->data);
        slot->data.relativeOffset = get_relative_offset(&slot->data, blob_g);
        register_roffset_slot(ptr_slot_g, blob_g);
        string_blob_phys_bytes_[blob_g] = alloc_bytes;
    }

    void string_clear(String& s)
    {
        goffset_t str_g = arena_byte_offset_of(&s);
        String* slot = reinterpret_cast<String*>(get_ptr_unsafe_to_store(str_g));
        const char* old_ptr = slot->data.get();
        if (old_ptr != nullptr)
        {
            ZMEYA_ASSERT(contains_pointer(old_ptr));
            goffset_t blob_g = arena_byte_offset_of(old_ptr);
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
        register_roffset_slot(arena_byte_offset_of(&slot->data), goffset_t(0));
    }
