namespace detail
{

template <typename T> void assign_pointer(BuilderBase& builder, zm::Pointer<T>& _to, T* from)
{
    goffset_t to_offset = builder.arena_byte_offset_of(&_to);
    if (from == nullptr)
    {
        zm::Pointer<T>* to = reinterpret_cast<zm::Pointer<T>*>(builder.get_ptr_unsafe_to_store(to_offset));
        to->relativeOffset = 0;
        builder.register_roffset_slot(to_offset, goffset_t(0));
        return;
    }
    ZMEYA_ASSERT(builder.contains_pointer(from) && "Pointer 'from' should belong to the builder");
    goffset_t fromOffset = builder.arena_byte_offset_of(from);
    zm::Pointer<T>* to = reinterpret_cast<zm::Pointer<T>*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->relativeOffset = builder.get_relative_offset(to, fromOffset);
    builder.register_roffset_slot(to_offset, fromOffset);
}

inline void assign_string_std(BuilderBase& builder, String& _to, const std::string& from)
{
    ScopedBuilder tls(&builder);
    goffset_t to_offset = builder.arena_byte_offset_of(&_to);
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
    builder.register_roffset_slot(builder.arena_byte_offset_of(&to->data), stringDataOffset);
    builder.string_blob_phys_register(stringDataOffset, len + 1);
}

inline void assign_string_cstr(BuilderBase& builder, String& _to, const char* from)
{
    ZMEYA_ASSERT(from != nullptr);
    ScopedBuilder tls(&builder);
    goffset_t to_offset = builder.arena_byte_offset_of(&_to);
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
    builder.register_roffset_slot(builder.arena_byte_offset_of(&to->data), stringDataOffset);
    builder.string_blob_phys_register(stringDataOffset, len + 1);
}

template <typename F, typename T> void deep_copy_into_offset(BuilderBase& builder, const F& from, goffset_t to_ofs)
{
    T* p_to = reinterpret_cast<T*>(builder.get_ptr_unsafe_to_store(to_ofs));
    BuilderBase::placementCtor<T>(p_to);
    if constexpr (std::is_same_v<T, zm::String> && std::is_same_v<std::decay_t<F>, std::string>)
    {
        assign_string_std(builder, *p_to, from);
    }
    else
    {
        ScopedBuilder scope(&builder);
        *p_to = from;
    }
}

} // namespace detail
