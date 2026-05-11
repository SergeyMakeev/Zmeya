template <typename T> template <typename U> inline void Array<T>::push_back(U&& v)
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_push_back(*this, std::forward<U>(v));
}

template <typename T> inline void Array<T>::pop_back()
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_pop_back(*this);
}

template <typename T> inline void Array<T>::clear()
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_clear(*this);
}

template <typename T> inline void Array<T>::erase_at(size_t index)
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_erase_at(*this, index);
}

template <typename T> inline void Array<T>::resize(size_t new_size, const T& fill)
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->array_resize_fill(*this, new_size, fill);
}

inline void String::clear()
{
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->string_clear(*this);
}

inline String& String::append(const char* suf, size_t suf_len)
{
    ZMEYA_ASSERT(suf != nullptr);
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->string_append_cstr(*this, suf, suf_len);
    return *this;
}

inline String& String::append(const char* suf)
{
    ZMEYA_ASSERT(suf != nullptr);
    return append(suf, std::strlen(suf));
}

inline String& String::operator+=(const char* suf) { return append(suf); }

template <typename Key> template <typename F> inline void HashSet<Key>::insert(const F& item) { zm::hashset_insert(*detail::get_global_builder(), *this, item); }

template <typename Key> template <typename F> inline void HashSet<Key>::erase(const F& key) { zm::hashset_erase(*detail::get_global_builder(), *this, key); }

template <typename Key> inline void HashSet<Key>::clear() { zm::hashset_clear(*detail::get_global_builder(), *this); }

template <typename K, typename V> template <typename FK, typename FV> inline void HashMap<K, V>::insert(const FK& key, const FV& val)
{
    zm::hashmap_insert(*detail::get_global_builder(), *this, key, val);
}

template <typename K, typename V> template <typename FK> inline void HashMap<K, V>::erase(const FK& key) { zm::hashmap_erase(*detail::get_global_builder(), *this, key); }

template <typename K, typename V> inline void HashMap<K, V>::clear() { zm::hashmap_clear(*detail::get_global_builder(), *this); }
