    template <typename T, typename U> void array_push_back(Array<T>& arr, U&& value)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value,
            "array_push_back is only supported for slab-memcpy-safe element types; use assign(vector<...>) for String, Pointer, nested Array, etc.");
        goffset_t hdr_g = arena_byte_offset_of(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        size_t old_n = size_t(slot->numElements);
        size_t new_n = old_n + 1;
        constexpr size_t alignOfT = std::alignment_of<T>::value;
        constexpr size_t sizeOfT = sizeof(T);

        size_t old_cap = old_n;
        const auto itPhys = array_slab_phys_bytes_.find(hdr_g);
        if (itPhys != array_slab_phys_bytes_.end())
        {
            ZMEYA_ASSERT(itPhys->second % sizeOfT == 0);
            old_cap = itPhys->second / sizeOfT;
        }

        if (old_cap > 0 && new_n <= old_cap)
        {
            slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
            T* base = slot->get_raw_ptr_unsafe_can_be_relocated();
            placementCtor<T>(&base[old_n], std::forward<U>(value));
            slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
            slot->numElements = uint32_t(new_n);
            return;
        }

        size_t new_cap = new_n;
        if (old_n == 0)
        {
            new_cap = (std::max)(size_t(8), new_n);
        }
        else
        {
            ZMEYA_ASSERT(old_cap >= old_n);
            ZMEYA_ASSERT(old_cap <= SIZE_MAX / 2);
            const size_t doubled = old_cap * 2;
            new_cap = (std::max)(new_n, doubled);
        }
        ZMEYA_ASSERT(new_cap <= SIZE_MAX / sizeOfT);

        size_t dead_slab_bytes = old_n * sizeOfT;
        if (itPhys != array_slab_phys_bytes_.end())
        {
            dead_slab_bytes = itPhys->second;
            array_slab_phys_bytes_.erase(itPhys);
        }
        if (old_n > 0)
        {
            T* old_data = slot->get_raw_ptr_unsafe_can_be_relocated();
            note_dead_range(arena_byte_offset_of(old_data), dead_slab_bytes);
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
        array_slab_phys_bytes_[hdr_g] = new_cap * sizeOfT;
    }

    template <typename T> void array_pop_back(Array<T>& arr)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value, "array_pop_back requires same constraints as push_back for this builder");
        goffset_t hdr_g = arena_byte_offset_of(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        if (slot->numElements == 0)
        {
            return;
        }
        slot->numElements -= 1;
    }

    template <typename T> void array_clear(Array<T>& arr)
    {
        goffset_t hdr_g = arena_byte_offset_of(&arr);
        Array<T>* slot = reinterpret_cast<Array<T>*>(get_ptr_unsafe_to_store(hdr_g));
        note_dead_array_slab_if_any(slot);
        slot->numElements = 0;
        slot->relativeOffset = 0;
        register_roffset_slot(hdr_g, goffset_t(0));
    }

    template <typename T> void array_erase_at(Array<T>& arr, size_t index)
    {
        static_assert(detail::zm_array_push_back_ok<T>::value, "array_erase_at requires same constraints as push_back for this builder");
        goffset_t hdr_g = arena_byte_offset_of(&arr);
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
        goffset_t hdr_g = arena_byte_offset_of(&arr);
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
