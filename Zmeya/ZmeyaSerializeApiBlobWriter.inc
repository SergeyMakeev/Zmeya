template <typename TRoot> inline void BlobWriter<TRoot>::string_append(String& s, const char* suf)
{
    ZMEYA_ASSERT(suf != nullptr);
    impl_->string_append_cstr(s, suf, std::strlen(suf));
}

template <typename TRoot> inline void BlobWriter<TRoot>::string_append(String& s, const char* suf, size_t suf_len)
{
    ZMEYA_ASSERT(suf != nullptr);
    impl_->string_append_cstr(s, suf, suf_len);
}

template <typename TRoot> inline void BlobWriter<TRoot>::string_clear(String& s) { impl_->string_clear(s); }

template <typename TRoot> template <typename T, typename U> inline void BlobWriter<TRoot>::array_push_back(Array<T>& a, U&& v) { impl_->array_push_back(a, std::forward<U>(v)); }

template <typename TRoot> template <typename T> inline void BlobWriter<TRoot>::array_pop_back(Array<T>& a) { impl_->array_pop_back(a); }

template <typename TRoot> template <typename T> inline void BlobWriter<TRoot>::array_clear(Array<T>& a) { impl_->array_clear(a); }

template <typename TRoot> template <typename T> inline void BlobWriter<TRoot>::array_erase_at(Array<T>& a, size_t index) { impl_->array_erase_at(a, index); }

template <typename TRoot> template <typename T> inline void BlobWriter<TRoot>::array_resize(Array<T>& a, size_t new_size, const T& fill) { impl_->array_resize_fill(a, new_size, fill); }

template <typename TRoot> template <typename Key, typename F> inline void BlobWriter<TRoot>::hashset_insert(HashSet<Key>& hs, const F& item) { zm::hashset_insert(*impl_, hs, item); }

template <typename TRoot> template <typename Key, typename F> inline void BlobWriter<TRoot>::hashset_erase(HashSet<Key>& hs, const F& key) { zm::hashset_erase(*impl_, hs, key); }

template <typename TRoot> template <typename Key> inline void BlobWriter<TRoot>::hashset_clear(HashSet<Key>& hs) { zm::hashset_clear(*impl_, hs); }

template <typename TRoot> template <typename K, typename V, typename FK, typename FV>
inline void BlobWriter<TRoot>::hashmap_insert(HashMap<K, V>& hm, const FK& key, const FV& val)
{
    zm::hashmap_insert(*impl_, hm, key, val);
}

template <typename TRoot> template <typename K, typename V, typename FK> inline void BlobWriter<TRoot>::hashmap_erase(HashMap<K, V>& hm, const FK& key)
{
    zm::hashmap_erase(*impl_, hm, key);
}

template <typename TRoot> template <typename K, typename V> inline void BlobWriter<TRoot>::hashmap_clear(HashMap<K, V>& hm) { zm::hashmap_clear(*impl_, hm); }
