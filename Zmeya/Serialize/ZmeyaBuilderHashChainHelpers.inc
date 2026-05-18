template <typename Key> inline size_t zm_hash_bucket_index_stored(const Key& k, size_t mod)
{
    if constexpr (std::is_same<Key, zm::String>::value)
    {
        return HashUtils::hashString(k.c_str()) % mod;
    }
    else
    {
        return HashUtils::hasher(k) % mod;
    }
}

template <typename F> inline size_t zm_hash_bucket_index_from_f(const F& f, size_t mod)
{
    if constexpr (std::is_same<F, std::string>::value)
    {
        return HashUtils::hashString(f.c_str()) % mod;
    }
    else
    {
        return HashUtils::hasher(f) % mod;
    }
}

template <typename Key, typename F> inline bool zm_chain_key_eq(const Key& a, const F& b)
{
    if constexpr (std::is_same<Key, zm::String>::value && std::is_same<F, std::string>::value)
    {
        return b == std::string(a.c_str());
    }
    else
    {
        return a == b;
    }
}
