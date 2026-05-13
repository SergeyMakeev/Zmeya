namespace detail
{

inline BuilderBase& require_tls_builder() noexcept
{
    BuilderBase* b = get_global_builder();
    ZMEYA_ASSERT(b != nullptr);
    return *b;
}

inline size_t string_slot_old_phys_bytes(BuilderBase& builder, const char* old_ptr)
{
    ZMEYA_ASSERT(old_ptr != nullptr);
    ZMEYA_ASSERT(builder.contains_pointer(old_ptr));
    const goffset_t old_blob_g = builder.arena_byte_offset_of(old_ptr);
    size_t old_phys_pre_dead = std::strlen(old_ptr) + 1;
    const auto pit = builder.string_blob_phys_bytes_.find(old_blob_g);
    if (pit != builder.string_blob_phys_bytes_.end())
    {
        old_phys_pre_dead = pit->second;
    }
    return old_phys_pre_dead;
}

inline size_t string_alloc_total_bytes(size_t need, bool had_old, size_t old_phys_pre_dead)
{
    if (!had_old)
    {
        constexpr size_t kMinFirstBlob = 32;
        return (std::max)(need, kMinFirstBlob);
    }
    size_t cap = (std::max)(old_phys_pre_dead, size_t(8));
    while (cap < need && cap <= SIZE_MAX / 2)
    {
        cap *= 2;
    }
    return (std::max)(need, cap);
}

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

    String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
    const char* old_ptr = to->data.get();
    const size_t len = from.size();
    ZMEYA_ASSERT(len <= SIZE_MAX - 1);
    const size_t need = len + 1;

    const bool had_old = old_ptr != nullptr;
    const size_t old_phys_pre_dead = had_old ? string_slot_old_phys_bytes(builder, old_ptr) : size_t(0);

    builder.string_note_dead_existing_blob(_to);

    to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));

    const size_t alloc_bytes = string_alloc_total_bytes(need, had_old, old_phys_pre_dead);

    zm::goffset_t stringDataOffset = builder.alloc_aligned(alloc_bytes, 1);
    char* stringData = reinterpret_cast<char*>(builder.get_ptr_unsafe_to_store(stringDataOffset));
    std::memcpy(stringData, from.data(), len);
    stringData[len] = '\0';
    to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->data.relativeOffset = builder.get_relative_offset(&to->data, stringDataOffset);
    builder.register_roffset_slot(builder.arena_byte_offset_of(&to->data), stringDataOffset);
    builder.string_blob_phys_register(stringDataOffset, alloc_bytes);
}

inline void assign_string_cstr(BuilderBase& builder, String& _to, const char* from)
{
    ZMEYA_ASSERT(from != nullptr);
    ScopedBuilder tls(&builder);
    goffset_t to_offset = builder.arena_byte_offset_of(&_to);
    const size_t len = std::strlen(from);
    if (len == 0)
    {
        String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
        if (!to->empty())
        {
            builder.string_clear(*to);
        }
        return;
    }

    String* to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
    const char* old_ptr = to->data.get();
    ZMEYA_ASSERT(len <= SIZE_MAX - 1);
    const size_t need = len + 1;

    const bool had_old = old_ptr != nullptr;
    const size_t old_phys_pre_dead = had_old ? string_slot_old_phys_bytes(builder, old_ptr) : size_t(0);

    builder.string_note_dead_existing_blob(_to);

    to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));

    const size_t alloc_bytes = string_alloc_total_bytes(need, had_old, old_phys_pre_dead);

    zm::goffset_t stringDataOffset = builder.alloc_aligned(alloc_bytes, 1);
    char* stringData = reinterpret_cast<char*>(builder.get_ptr_unsafe_to_store(stringDataOffset));
    std::memcpy(stringData, from, len);
    stringData[len] = '\0';
    to = reinterpret_cast<String*>(builder.get_ptr_unsafe_to_store(to_offset));
    to->data.relativeOffset = builder.get_relative_offset(&to->data, stringDataOffset);
    builder.register_roffset_slot(builder.arena_byte_offset_of(&to->data), stringDataOffset);
    builder.string_blob_phys_register(stringDataOffset, alloc_bytes);
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
