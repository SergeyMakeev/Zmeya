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
                goffset_t baseG = arena_byte_offset_of(slotPtr);
                diff_t d = diff_t(goffset_t(target_g)) - diff_t(goffset_t(baseG));
                ZMEYA_ASSERT(d >= diff_t(std::numeric_limits<roffset_t>::min()));
                ZMEYA_ASSERT(d <= diff_t(std::numeric_limits<roffset_t>::max()));
                *pr = roffset_t(d);
            }
        }
    }
