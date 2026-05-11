namespace detail
{

template <typename Key, typename F> void hashset_stl_fill_from_zm(std::unordered_set<F>& out, const HashSet<Key>& hs)
{
    for (const Key& k : hs)
    {
        if constexpr (std::is_same<Key, String>::value && std::is_same<F, std::string>::value)
        {
            out.insert(std::string(k.c_str()));
        }
        else
        {
            out.insert(static_cast<F>(k));
        }
    }
}

template <typename Key, typename F, typename SkipKey>
void hashset_stl_fill_from_zm_skipping(std::unordered_set<F>& out, const HashSet<Key>& hs, const SkipKey& skip)
{
    for (const Key& k : hs)
    {
        if (k == skip)
        {
            continue;
        }
        if constexpr (std::is_same<Key, String>::value && std::is_same<F, std::string>::value)
        {
            out.insert(std::string(k.c_str()));
        }
        else
        {
            out.insert(static_cast<F>(k));
        }
    }
}

template <typename K, typename V, typename FK, typename FV>
bool hashmap_try_in_place_update(HashMap<K, V>& hm, const FK& key, const FV& val)
{
    if constexpr (std::is_same_v<K, String> && std::is_same_v<FK, std::string>)
    {
        if (V* pv = hm.find(key.c_str()))
        {
            *pv = val;
            return true;
        }
    }
    else if constexpr (std::is_same_v<FK, K>)
    {
        if (V* pv = hm.find(key))
        {
            *pv = val;
            return true;
        }
    }
    return false;
}

template <typename K, typename V, typename MapTmp, typename FK>
void hashmap_stl_fill_from_zm_excluding(MapTmp& out, const HashMap<K, V>& hm, const FK& key)
{
    for (const auto& it : hm)
    {
        if (it.first == key)
        {
            continue;
        }
        if constexpr (std::is_same<K, String>::value)
        {
            out.emplace(std::string(it.first.c_str()), it.second);
        }
        else
        {
            out.emplace(it.first, it.second);
        }
    }
}

template <typename K, typename V, typename MapTmp> void hashmap_stl_import_from_zm(MapTmp& out, const HashMap<K, V>& hm)
{
    for (const auto& it : hm)
    {
        if constexpr (std::is_same_v<K, String>)
        {
            out.emplace(std::string(it.first.c_str()), it.second);
        }
        else
        {
            out.emplace(it.first, it.second);
        }
    }
}

} // namespace detail

// Incremental hash: slab-safe keys use chain APIs in ZmeyaBuilderHashChain.inc; other keys resnapshot via STL + impl_assign_*.

template <typename Key, typename F> void hashset_insert(detail::BuilderBase& builder, HashSet<Key>& hs, const F& item)
{
    if constexpr (zm_hashset_chain_incremental_ok<Key>::value)
    {
        if constexpr (std::is_same_v<Key, F>)
        {
            if (hs.contains(item))
            {
                return;
            }
        }
        else
        {
            const Key k = static_cast<Key>(item);
            if (hs.contains(k))
            {
                return;
            }
        }
        builder.hashset_chain_insert(hs, item);
        return;
    }
    if constexpr (std::is_same_v<Key, String> && std::is_same_v<F, std::string>)
    {
        if (hs.contains(item.c_str()))
        {
            return;
        }
    }
    else if constexpr (std::is_same_v<Key, F>)
    {
        if (hs.contains(item))
        {
            return;
        }
    }
    std::unordered_set<F> tmp;
    tmp.reserve(hs.size() + 16);
    detail::hashset_stl_fill_from_zm(tmp, hs);
    tmp.insert(item);
    builder.impl_assign_hashset(hs, tmp);
}

template <typename Key, typename F> void hashset_erase(detail::BuilderBase& builder, HashSet<Key>& hs, const F& key)
{
    if constexpr (zm_hashset_chain_incremental_ok<Key>::value)
    {
        builder.hashset_chain_erase(hs, key);
        return;
    }
    std::unordered_set<F> tmp;
    tmp.reserve(hs.size());
    detail::hashset_stl_fill_from_zm_skipping(tmp, hs, key);
    builder.impl_assign_hashset(hs, tmp);
}

template <typename Key> void hashset_clear(detail::BuilderBase& builder, HashSet<Key>& hs)
{
    if constexpr (zm_hashset_chain_incremental_ok<Key>::value)
    {
        builder.hashset_chain_clear(hs);
        return;
    }
    if constexpr (std::is_same<Key, String>::value)
    {
        builder.impl_assign_hashset(hs, std::unordered_set<std::string>{});
    }
    else
    {
        builder.impl_assign_hashset(hs, std::unordered_set<Key>{});
    }
}

template <typename K, typename V, typename FK, typename FV> void hashmap_insert(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key, const FV& val)
{
    if (detail::hashmap_try_in_place_update<K, V, FK, FV>(hm, key, val))
    {
        return;
    }
    if constexpr (zm_hashmap_chain_incremental_ok<K, V>::value)
    {
        builder.hashmap_chain_insert(hm, key, val);
        return;
    }
    // O(hm.size()) snapshot + full rebuild; bulk assign is cheaper for large maps.
    if constexpr (std::is_same<K, String>::value)
    {
        std::unordered_map<std::string, V> tmp;
        tmp.reserve(hm.size() + 8);
        detail::hashmap_stl_import_from_zm(tmp, hm);
        tmp[key] = val;
        builder.impl_assign_hashmap(hm, tmp);
    }
    else
    {
        std::unordered_map<K, V> tmp;
        tmp.reserve(hm.size() + 8);
        detail::hashmap_stl_import_from_zm(tmp, hm);
        tmp[key] = val;
        builder.impl_assign_hashmap(hm, tmp);
    }
}

template <typename K, typename V, typename FK> void hashmap_erase(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key)
{
    if constexpr (zm_hashmap_chain_incremental_ok<K, V>::value)
    {
        builder.hashmap_chain_erase(hm, key);
        return;
    }
    if constexpr (std::is_same<K, String>::value)
    {
        std::unordered_map<std::string, V> tmp;
        tmp.reserve(hm.size());
        detail::hashmap_stl_fill_from_zm_excluding(tmp, hm, key);
        builder.impl_assign_hashmap(hm, tmp);
    }
    else
    {
        std::unordered_map<K, V> tmp;
        for (const auto& it : hm)
        {
            if (it.first == key)
            {
                continue;
            }
            tmp.emplace(it.first, it.second);
        }
        builder.impl_assign_hashmap(hm, tmp);
    }
}

template <typename K, typename V> void hashmap_clear(detail::BuilderBase& builder, HashMap<K, V>& hm)
{
    if constexpr (zm_hashmap_chain_incremental_ok<K, V>::value)
    {
        builder.hashmap_chain_clear(hm);
        return;
    }
    if constexpr (std::is_same<K, String>::value)
    {
        builder.impl_assign_hashmap(hm, std::unordered_map<std::string, V>{});
    }
    else
    {
        builder.impl_assign_hashmap(hm, std::unordered_map<K, V>{});
    }
}
