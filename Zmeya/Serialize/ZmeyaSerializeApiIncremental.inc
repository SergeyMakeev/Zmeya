namespace detail
{

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

} // namespace detail

template <typename Key, typename F> void hashset_insert(detail::BuilderBase& builder, HashSet<Key>& hs, const F& item)
{
    if constexpr (std::is_same_v<Key, F>)
    {
        if (hs.contains(item))
        {
            return;
        }
    }
    else if constexpr (std::is_same_v<Key, String> && std::is_same_v<F, std::string>)
    {
        if (hs.contains(item.c_str()))
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
}

template <typename Key, typename F> void hashset_erase(detail::BuilderBase& builder, HashSet<Key>& hs, const F& key)
{
    builder.hashset_chain_erase(hs, key);
}

template <typename Key> void hashset_clear(detail::BuilderBase& builder, HashSet<Key>& hs)
{
    builder.hashset_chain_clear(hs);
}

template <typename K, typename V, typename FK, typename FV> void hashmap_insert(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key, const FV& val)
{
    if (detail::hashmap_try_in_place_update<K, V, FK, FV>(hm, key, val))
    {
        return;
    }
    builder.hashmap_chain_insert(hm, key, val);
}

template <typename K, typename V, typename FK> void hashmap_erase(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key)
{
    builder.hashmap_chain_erase(hm, key);
}

template <typename K, typename V> void hashmap_clear(detail::BuilderBase& builder, HashMap<K, V>& hm)
{
    builder.hashmap_chain_clear(hm);
}

template <typename Key> void hashset_reserve_nodes(detail::BuilderBase& builder, HashSet<Key>& hs, size_t min_node_pool_capacity)
{
    builder.hashset_chain_reserve_node_pool(hs, min_node_pool_capacity);
}

template <typename K, typename V> void hashmap_reserve_nodes(detail::BuilderBase& builder, HashMap<K, V>& hm, size_t min_node_pool_capacity)
{
    builder.hashmap_chain_reserve_node_pool(hm, min_node_pool_capacity);
}
