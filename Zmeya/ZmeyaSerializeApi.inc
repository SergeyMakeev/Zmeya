/*

**TLS-facing serialization API**

Assignments, incremental container edits, `deep_copy`, and `BlobWriter` method bodies. Kept in one
translation unit fragment so call sites only include `ZmeyaSerializeApi.h`.

These operators bridge STL-shaped values into blob-relative `zm::` fields using the active builder in
TLS (or an explicit `BuilderBase&` overload).

*/

// Forward declarations for deep-copy functions (specialized overloads only)
inline void deep_copy(const std::string& from, zm::goffset_t to_ofs);
inline void deep_copy(const char* from, zm::goffset_t to_ofs);
template <typename F, typename T> void deep_copy(const std::vector<F>& from, zm::goffset_t to_ofs);
template <typename F, typename Key> void deep_copy(const std::unordered_set<F>& from, zm::goffset_t to_ofs);
template <typename FK, typename FV, typename Key, typename Value> void deep_copy(const std::unordered_map<FK, FV>& from, zm::goffset_t to_ofs);
template <typename F1, typename F2, typename T1, typename T2> void deep_copy(const Pair<F1, F2>& from, zm::goffset_t to_ofs);
template <typename F, typename T> void deep_copy(detail::BuilderBase& builder, const F& from, zm::goffset_t to_ofs);

namespace detail
{

template <typename T> void assign_pointer(BuilderBase& builder, zm::Pointer<T>& _to, T* from)
{
    goffset_t to_offset = builder.get_global_offset(&_to);
    if (from == nullptr)
    {
        zm::Pointer<T>* to = reinterpret_cast<zm::Pointer<T>*>(builder.get_ptr_unsafe_to_store(to_offset));
        to->relativeOffset = 0;
        builder.register_roffset_slot(to_offset, goffset_t(0));
        return;
    }
    ZMEYA_ASSERT(builder.contains_pointer(from) && "Pointer 'from' should belong to the builder");
    goffset_t fromOffset = builder.get_global_offset(from);
    zm::Pointer<T>* to = reinterpret_cast<zm::Pointer<T>*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->relativeOffset = builder.get_relative_offset(to, fromOffset);
    builder.register_roffset_slot(to_offset, fromOffset);
}

inline void assign_string_std(BuilderBase& builder, String& _to, const std::string& from)
{
    ScopedBuilder tls(&builder);
    goffset_t to_offset = builder.get_global_offset(&_to);
    if (from.empty())
    {
        String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
        if (!to->empty())
        {
            builder.string_clear(*to);
        }
        return;
    }
    builder.string_note_dead_existing_blob(_to);
    size_t len = from.size();
    ZMEYA_ASSERT(len <= SIZE_MAX - 1);
    zm::goffset_t stringDataOffset = builder.alloc_aligned(len + 1, 1);
    char* stringData = reinterpret_cast<char*>(builder.get_ptr_unsafe_to_store(stringDataOffset));
    std::memcpy(stringData, from.data(), len);
    stringData[len] = '\0';
    String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->data.relativeOffset = builder.get_relative_offset(&to->data, stringDataOffset);
    builder.register_roffset_slot(builder.get_global_offset(&to->data), stringDataOffset);
    builder.string_blob_phys_register(stringDataOffset, len + 1);
}

inline void assign_string_cstr(BuilderBase& builder, String& _to, const char* from)
{
    ZMEYA_ASSERT(from != nullptr);
    ScopedBuilder tls(&builder);
    goffset_t to_offset = builder.get_global_offset(&_to);
    size_t len = std::strlen(from);
    if (len == 0)
    {
        String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
        if (!to->empty())
        {
            builder.string_clear(*to);
        }
        return;
    }
    builder.string_note_dead_existing_blob(_to);
    ZMEYA_ASSERT(len <= SIZE_MAX - 1);
    zm::goffset_t stringDataOffset = builder.alloc_aligned(len + 1, 1);
    char* stringData = reinterpret_cast<char*>(builder.get_ptr_unsafe_to_store(stringDataOffset));
    std::memcpy(stringData, from, len);
    stringData[len] = '\0';
    String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->data.relativeOffset = builder.get_relative_offset(&to->data, stringDataOffset);
    builder.register_roffset_slot(builder.get_global_offset(&to->data), stringDataOffset);
    builder.string_blob_phys_register(stringDataOffset, len + 1);
}

} // namespace detail

template <typename T> void assign(zm::Pointer<T>& _to, T* from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    detail::assign_pointer(*builder, _to, from);
}

template <typename T> void assign(Array<zm::Pointer<T>>& _to, const std::vector<T*>& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    builder->impl_assign_pointer_array(_to, from);
}

inline void assign(String& _to, const std::string& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    detail::assign_string_std(*builder, _to, from);
}

inline void assign(String& _to, const char* from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    detail::assign_string_cstr(*builder, _to, from);
}

template <typename T, typename F> void assign(Array<T>& _to, const std::vector<F>& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    builder->impl_assign_array_vector(_to, from);
}

// HashSet conversions - standalone implementation
template <typename Key, typename F> void assign(HashSet<Key>& _to, const std::unordered_set<F>& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    builder->impl_assign_hashset(_to, from);
}

// HashMap conversions - standalone implementation
template <typename Key, typename Value, typename FK, typename FV>
void assign(HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    builder->impl_assign_hashmap(_to, from);
}

// Deep-copy adapter implementations

// Universal deep_copy - works for all types thanks to operator= overloads
template <typename F, typename T> void deep_copy(const F& from, zm::goffset_t to_ofs)
{
    detail::BuilderBase* builder = detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    T* p_to = reinterpret_cast<T*>(builder->get_ptr_unsafe_to_store(to_ofs));
    detail::BuilderBase::placementCtor<T>(p_to);
    *p_to = from; // This will call the appropriate operator= automatically!
}

template <typename F, typename T> void deep_copy(detail::BuilderBase& builder, const F& from, zm::goffset_t to_ofs)
{
    T* p_to = reinterpret_cast<T*>(builder.get_ptr_unsafe_to_store(to_ofs));
    detail::BuilderBase::placementCtor<T>(p_to);
    detail::ScopedBuilder scope(&builder);
    *p_to = from;
}

template <typename T> void assign(detail::BuilderBase& builder, zm::Pointer<T>& _to, T* from)
{
    detail::assign_pointer(builder, _to, from);
}

template <typename T> void assign(detail::BuilderBase& builder, Array<zm::Pointer<T>>& _to, const std::vector<T*>& from)
{
    builder.impl_assign_pointer_array(_to, from);
}

inline void assign(detail::BuilderBase& builder, String& _to, const std::string& from)
{
    detail::assign_string_std(builder, _to, from);
}

inline void assign(detail::BuilderBase& builder, String& _to, const char* from)
{
    detail::assign_string_cstr(builder, _to, from);
}

template <typename T, typename F> void assign(detail::BuilderBase& builder, Array<T>& _to, const std::vector<F>& from)
{
    builder.impl_assign_array_vector(_to, from);
}

template <typename Key, typename F> void assign(detail::BuilderBase& builder, HashSet<Key>& _to, const std::unordered_set<F>& from)
{
    builder.impl_assign_hashset(_to, from);
}

template <typename Key, typename Value, typename FK, typename FV>
void assign(detail::BuilderBase& builder, HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from)
{
    builder.impl_assign_hashmap(_to, from);
}

template <typename Key, typename F> void hashset_insert(detail::BuilderBase& builder, HashSet<Key>& hs, const F& item)
{
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
    for (const Key& k : hs)
    {
        if constexpr (std::is_same<Key, String>::value && std::is_same<F, std::string>::value)
        {
            tmp.insert(std::string(k.c_str()));
        }
        else
        {
            tmp.insert(static_cast<F>(k));
        }
    }
    tmp.insert(item);
    builder.impl_assign_hashset(hs, tmp);
}

template <typename Key, typename F> void hashset_erase(detail::BuilderBase& builder, HashSet<Key>& hs, const F& key)
{
    std::unordered_set<F> tmp;
    tmp.reserve(hs.size());
    for (const Key& k : hs)
    {
        bool skip = false;
        if constexpr (std::is_same<Key, String>::value && std::is_same<F, std::string>::value)
        {
            skip = (key == std::string(k.c_str()));
        }
        else
        {
            skip = (k == key);
        }
        if (skip)
        {
            continue;
        }
        if constexpr (std::is_same<Key, String>::value && std::is_same<F, std::string>::value)
        {
            tmp.insert(std::string(k.c_str()));
        }
        else
        {
            tmp.insert(static_cast<F>(k));
        }
    }
    builder.impl_assign_hashset(hs, tmp);
}

template <typename Key> void hashset_clear(detail::BuilderBase& builder, HashSet<Key>& hs)
{
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
    if constexpr (std::is_same_v<K, String> && std::is_same_v<FK, std::string>)
    {
        if (V* pv = hm.find(key.c_str()))
        {
            *pv = val;
            return;
        }
    }
    else if constexpr (std::is_same_v<FK, K>)
    {
        if (V* pv = hm.find(key))
        {
            *pv = val;
            return;
        }
    }
    // Snapshot the full map and re-assign each call (O(hm.size())); large batches should use bulk assign instead.
    if constexpr (std::is_same<K, String>::value)
    {
        std::unordered_map<std::string, V> tmp;
        tmp.reserve(hm.size() + 8);
        for (const auto& it : hm)
        {
            tmp[std::string(it.first.c_str())] = it.second;
        }
        tmp[key] = val;
        builder.impl_assign_hashmap(hm, tmp);
    }
    else
    {
        std::unordered_map<K, V> tmp;
        tmp.reserve(hm.size() + 8);
        for (const auto& it : hm)
        {
            tmp[it.first] = it.second;
        }
        tmp[key] = val;
        builder.impl_assign_hashmap(hm, tmp);
    }
}

template <typename K, typename V, typename FK> void hashmap_erase(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key)
{
    if constexpr (std::is_same<K, String>::value)
    {
        std::unordered_map<std::string, V> tmp;
        for (const auto& it : hm)
        {
            if (key == std::string(it.first.c_str()))
            {
                continue;
            }
            tmp.emplace(std::string(it.first.c_str()), it.second);
        }
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
    if constexpr (std::is_same<K, String>::value)
    {
        builder.impl_assign_hashmap(hm, std::unordered_map<std::string, V>{});
    }
    else
    {
        builder.impl_assign_hashmap(hm, std::unordered_map<K, V>{});
    }
}

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

inline String& String::append(const char* suf)
{
    ZMEYA_ASSERT(suf != nullptr);
    detail::BuilderBase* b = detail::get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    b->string_append_cstr(*this, suf, std::strlen(suf));
    return *this;
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

template <typename TRoot> inline void BlobWriter<TRoot>::string_append(String& s, const char* suf)
{
    ZMEYA_ASSERT(suf != nullptr);
    impl_->string_append_cstr(s, suf, std::strlen(suf));
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

template <typename Key, typename F> void hashset_insert(detail::BuilderBase& builder, HashSet<Key>& hs, const F& item);

template <typename Key, typename F> void hashset_erase(detail::BuilderBase& builder, HashSet<Key>& hs, const F& key);

template <typename Key> void hashset_clear(detail::BuilderBase& builder, HashSet<Key>& hs);

template <typename K, typename V, typename FK, typename FV> void hashmap_insert(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key, const FV& val);

template <typename K, typename V, typename FK> void hashmap_erase(detail::BuilderBase& builder, HashMap<K, V>& hm, const FK& key);

template <typename K, typename V> void hashmap_clear(detail::BuilderBase& builder, HashMap<K, V>& hm);

/*

**Helper syntax for clean assignments**

Container and **`zm::Pointer`** types expose **`operator=`** from STL-shaped values (and raw pointers for **`Pointer`**). Prefer normal assignments inside **`zm::write_blob`**.

Usage examples:
  root->string_array = src_vector;

*/

#define ZM_ASSIGN(zm_var, std_var) ((zm_var) = (std_var))
