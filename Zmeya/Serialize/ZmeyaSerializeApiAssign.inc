template <typename T> void assign(zm::Pointer<T>& _to, T* from)
{
    detail::assign_pointer(detail::require_tls_builder(), _to, from);
}

template <typename T> void assign(Array<zm::Pointer<T>>& _to, const std::vector<T*>& from)
{
    detail::require_tls_builder().impl_assign_pointer_array(_to, from);
}

inline void assign(String& _to, const std::string& from)
{
    detail::assign_string_std(detail::require_tls_builder(), _to, from);
}

inline void assign(String& _to, const char* from)
{
    detail::assign_string_cstr(detail::require_tls_builder(), _to, from);
}

template <typename T, typename F> void assign(Array<T>& _to, const std::vector<F>& from)
{
    detail::require_tls_builder().impl_assign_array_vector(_to, from);
}

template <typename Key, typename F> void assign(HashSet<Key>& _to, const std::unordered_set<F>& from)
{
    detail::require_tls_builder().impl_assign_hashset(_to, from);
}

template <typename Key, typename Value, typename FK, typename FV>
void assign(HashMap<Key, Value>& _to, const std::unordered_map<FK, FV>& from)
{
    detail::require_tls_builder().impl_assign_hashmap(_to, from);
}

template <typename F, typename T> void deep_copy(const F& from, zm::goffset_t to_ofs)
{
    static_assert(std::is_trivially_copyable<T>::value, "deep_copy target T must be trivially copyable (blob field)");
    detail::deep_copy_into_offset<F, T>(detail::require_tls_builder(), from, to_ofs);
}

template <typename F, typename T> void deep_copy(detail::BuilderBase& builder, const F& from, zm::goffset_t to_ofs)
{
    static_assert(std::is_trivially_copyable<T>::value, "deep_copy target T must be trivially copyable (blob field)");
    detail::deep_copy_into_offset<F, T>(builder, from, to_ofs);
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
