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

        if (!hash_chain_nodes_phys_bytes_.empty())
        {
            std::unordered_map<goffset_t, size_t> newPhys;
            newPhys.reserve(hash_chain_nodes_phys_bytes_.size() + 1);
            for (const auto& kv : hash_chain_nodes_phys_bytes_)
            {
                newPhys[remap_one(kv.first)] = kv.second;
            }
            hash_chain_nodes_phys_bytes_ = std::move(newPhys);
        }
        if (!hash_chain_nodes_first_alloc_hint_.empty())
        {
            std::unordered_map<goffset_t, size_t> newHint;
            newHint.reserve(hash_chain_nodes_first_alloc_hint_.size() + 1);
            for (const auto& kv : hash_chain_nodes_first_alloc_hint_)
            {
                newHint[remap_one(kv.first)] = kv.second;
            }
            hash_chain_nodes_first_alloc_hint_ = std::move(newHint);
        }

        if (!array_slab_phys_bytes_.empty())
        {
            std::unordered_map<goffset_t, size_t> newArrayPhys;
            newArrayPhys.reserve(array_slab_phys_bytes_.size() + 1);
            for (const auto& kv : array_slab_phys_bytes_)
            {
                newArrayPhys[remap_one(kv.first)] = kv.second;
            }
            array_slab_phys_bytes_ = std::move(newArrayPhys);
        }

        data.swap(newData);
#if defined(_DEBUG) || defined(ZMEYA_DEBUG_ARENA_EPOCH)
        ++arena_realloc_epoch_;
#endif
        dead_ranges_.clear();
        string_blob_phys_bytes_.clear();
    }
