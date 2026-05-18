/*

**Blob writing**

User-facing entry is **`zm::write_scope`** and **`BlobWriter`** only. **`w.root()`** and **`w.allocate<T>()`** return **`zm::ArenaRef<T>`** (see **`ZmeyaArenaRef.h`**). Implementation types live in **`zm::detail`**.

*/

namespace detail
{
template <typename TRoot, typename Fn>
BlobBuffer write_scope_with_initial_buffer_bytes(Fn&& fn, size_t initialBufferBytes, size_t finalizeAlignment);
}

template <typename TRoot, typename Fn>
BlobBuffer write_scope(Fn&& fn, size_t finalizeAlignment);

template <typename TRoot>
class BlobWriter
{
    struct Private {};

    template <typename R, typename Fn>
    friend BlobBuffer write_scope(Fn&& fn, size_t finalizeAlignment);

    template <typename R, typename Fn>
    friend BlobBuffer detail::write_scope_with_initial_buffer_bytes(Fn&& fn, size_t initialBufferBytes, size_t finalizeAlignment);

    explicit BlobWriter(detail::Builder<TRoot>* impl, Private)
        : impl_(impl)
    {
    }

    detail::Builder<TRoot>* impl_;

  public:
    ZMEYA_NODISCARD ArenaRef<TRoot> root() const noexcept { return impl_->getRoot(); }

    template <typename T>
    ZMEYA_NODISCARD ArenaRef<T> allocate() { return detail::allocate_arena_ref<T>(impl_); }

    ZMEYA_NODISCARD bool contains_pointer(const void* ptr) const { return impl_->contains_pointer(ptr); }

    ZMEYA_NODISCARD detail::BuilderBase* builder_base() const noexcept { return impl_; }

    void string_append(String& s, const char* suf);

    void string_append(String& s, const char* suf, size_t suf_len);

    void string_clear(String& s);

    template <typename T, typename U> void array_push_back(Array<T>& a, U&& v);

    template <typename T> void array_pop_back(Array<T>& a);

    template <typename T> void array_clear(Array<T>& a);

    template <typename T> void array_erase_at(Array<T>& a, size_t index);

    template <typename T> void array_resize(Array<T>& a, size_t new_size, const T& fill);

    template <typename Key, typename F> void hashset_insert(HashSet<Key>& hs, const F& item);

    template <typename Key, typename F> void hashset_erase(HashSet<Key>& hs, const F& key);

    template <typename Key> void hashset_clear(HashSet<Key>& hs);

    template <typename Key> void hashset_reserve_nodes(HashSet<Key>& hs, size_t min_node_pool_capacity);

    template <typename K, typename V, typename FK, typename FV> void hashmap_insert(HashMap<K, V>& hm, const FK& key, const FV& val);

    template <typename K, typename V, typename FK> void hashmap_erase(HashMap<K, V>& hm, const FK& key);

    template <typename K, typename V> void hashmap_clear(HashMap<K, V>& hm);

    template <typename K, typename V> void hashmap_reserve_nodes(HashMap<K, V>& hm, size_t min_node_pool_capacity);
};

/*

**write_scope**

Installs TLS, invokes **`fn(writer)`**, then finalizes. **`BlobWriter`** is the stable handle so internals can evolve without changing call sites.
Returns **`zm::BlobBuffer`** (same backing allocator as the arena) by **moving** the finalized bytes; no extra **`std::vector<char>`** copy of the span.

*/

template <typename TRoot, typename Fn>
ZMEYA_NODISCARD inline BlobBuffer write_scope(Fn&& fn, size_t finalizeAlignment = 4)
{
    return detail::write_scope_with_initial_buffer_bytes<TRoot>(std::forward<Fn>(fn), detail::kDefaultWriteBlobArenaReserveBytes, finalizeAlignment);
}

namespace detail
{

/*

**write_scope_with_initial_buffer_bytes**

Same session as **`zm::write_scope`**, but **`initialBufferBytes`** seeds **`std::vector<char>::reserve`** for the arena.
Intended for unit tests that intentionally force **`std::vector`** reallocations; application code should use **`zm::write_scope`** and avoid caching raw pointers across growth regardless of starting capacity.

*/

template <typename TRoot, typename Fn>
ZMEYA_NODISCARD inline BlobBuffer write_scope_with_initial_buffer_bytes(Fn&& fn, size_t initialBufferBytes, size_t finalizeAlignment)
{
    std::unique_ptr<Builder<TRoot>> builder = Builder<TRoot>::create(initialBufferBytes);
    ScopedBuilder scope(builder.get());
    BlobWriter<TRoot> writer(builder.get(), typename BlobWriter<TRoot>::Private{});
    std::forward<Fn>(fn)(writer);
    return builder->finalize_move_out(finalizeAlignment);
}

} // namespace detail
