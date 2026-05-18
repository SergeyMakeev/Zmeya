#pragma once

#include "ZmeyaHashMap.h"
#include <type_traits>
#include <utility>
#include <vector>

namespace zm
{

/*

**Blob layout validation (untrusted bytes)**

Call `validate_blob_view` (or `as_root_blob`) before treating external bytes as `TRoot*`. The read-side
`zm::` views stay unchecked for performance once you accept the span.

Composite struct roots need either `blob_root_deep_validate<TRoot>` (see below) or `validate_blob_view_strict` / `as_root_blob_strict` after you opt in with a specialization.

`schema_id` is reserved for application-specific versioning; the default validator ignores it.

*/

enum class BlobViewError : uint8_t
{
    Ok = 0,
    SpanTooSmall,
    BadAlignment,
    BadArray,
    BadPointer,
    BadString,
    BadHashChain,
    BadNested,
};

struct BlobLayoutValidator
{
  private:
    template <typename, typename = void> struct is_zm_pointer : std::false_type
    {
    };
    template <typename U> struct is_zm_pointer<Pointer<U>> : std::true_type
    {
    };

    template <typename, typename = void> struct is_zm_array : std::false_type
    {
    };
    template <typename U> struct is_zm_array<Array<U>> : std::true_type
    {
    };

    template <typename, typename = void> struct is_zm_hashset : std::false_type
    {
    };
    template <typename U> struct is_zm_hashset<HashSet<U>> : std::true_type
    {
    };

    template <typename, typename = void> struct is_zm_hashmap : std::false_type
    {
    };
    template <typename K, typename V> struct is_zm_hashmap<HashMap<K, V>> : std::true_type
    {
    };

    template <typename Table, typename OnLiveNode>
    static bool hash_chain_graph_validate(const std::byte* blob_begin, size_t blob_size, const Table& table, OnLiveNode&& onLiveNode) noexcept
    {
        const uintptr_t b = reinterpret_cast<uintptr_t>(blob_begin);
        const uintptr_t e = b + blob_size;
        const uintptr_t th = reinterpret_cast<uintptr_t>(&table);
        if (th < b || th + sizeof(Table) > e)
        {
            return false;
        }
        if (!array(blob_begin, blob_size, table.buckets))
        {
            return false;
        }
        if (!array(blob_begin, blob_size, table.nodes))
        {
            return false;
        }
        const size_t nodePool = table.nodes.size();
        if (nodePool >= size_t(ZMEYA_HASH_CHAIN_NIL))
        {
            return false;
        }
        std::vector<unsigned char> visited;
        try
        {
            visited.assign(nodePool, 0);
        }
        catch (...)
        {
            return false;
        }
        uint32_t chainLive = 0;
        const size_t numBuckets = table.buckets.size();
        const auto* pool = table.nodes.getConstData();
        for (size_t bi = 0; bi < numBuckets; ++bi)
        {
            const size_t maxSteps = nodePool + 1;
            size_t steps = 0;
            for (uint32_t cur = table.buckets.getConstData()[bi].head; cur != ZMEYA_HASH_CHAIN_NIL; cur = pool[cur].next)
            {
                if (++steps > maxSteps)
                {
                    return false;
                }
                if (size_t(cur) >= nodePool)
                {
                    return false;
                }
                if (visited[size_t(cur)] != 0)
                {
                    return false;
                }
                visited[size_t(cur)] = 1;
                ++chainLive;
            }
        }
        if (chainLive != table.live_count_)
        {
            return false;
        }
        size_t freeSteps = 0;
        for (uint32_t cur = table.free_head_; cur != ZMEYA_HASH_CHAIN_NIL; cur = pool[cur].next)
        {
            if (++freeSteps > nodePool + 1)
            {
                return false;
            }
            if (size_t(cur) >= nodePool)
            {
                return false;
            }
            if (visited[size_t(cur)] == 1)
            {
                return false;
            }
            if (visited[size_t(cur)] == 2)
            {
                return false;
            }
            visited[size_t(cur)] = 2;
        }
        for (size_t i = 0; i < nodePool; ++i)
        {
            if (visited[i] == 1)
            {
                if (!std::forward<OnLiveNode>(onLiveNode)(pool[i]))
                {
                    return false;
                }
            }
        }
        return true;
    }

  public:
    template <typename T> static bool pointer(const std::byte* blob_begin, size_t blob_size, const Pointer<T>& p) noexcept
    {
        const uintptr_t b = reinterpret_cast<uintptr_t>(blob_begin);
        const uintptr_t e = b + blob_size;
        const uintptr_t slot = reinterpret_cast<uintptr_t>(&p);
        if (slot < b || slot + sizeof(Pointer<T>) > e)
        {
            return false;
        }
        if (p.relativeOffset == 0)
        {
            return true;
        }
        uintptr_t addr = 0;
        if (!detail::self_rel_target_address(slot, p.relativeOffset, &addr))
        {
            return false;
        }
        if (addr < b || addr + sizeof(T) > e)
        {
            return false;
        }
        if ((addr % alignof(T)) != 0)
        {
            return false;
        }
        const T& at = *reinterpret_cast<const T*>(addr);
        return field_dispatch(blob_begin, blob_size, at);
    }

    static bool string_payload(const std::byte* blob_begin, size_t blob_size, const Pointer<char>& p) noexcept
    {
        const uintptr_t b = reinterpret_cast<uintptr_t>(blob_begin);
        const uintptr_t e = b + blob_size;
        const uintptr_t slot = reinterpret_cast<uintptr_t>(&p);
        if (slot < b || slot + sizeof(Pointer<char>) > e)
        {
            return false;
        }
        if (p.relativeOffset == 0)
        {
            return true;
        }
        uintptr_t addr = 0;
        if (!detail::self_rel_target_address(slot, p.relativeOffset, &addr))
        {
            return false;
        }
        if (addr < b || addr >= e)
        {
            return false;
        }
        const char* c = reinterpret_cast<const char*>(addr);
        const size_t remaining = size_t(e - addr);
        for (size_t i = 0; i < remaining; ++i)
        {
            if (c[i] == '\0')
            {
                return true;
            }
        }
        return false;
    }

    static bool string(const std::byte* blob_begin, size_t blob_size, const String& s) noexcept
    {
        const uintptr_t b = reinterpret_cast<uintptr_t>(blob_begin);
        const uintptr_t e = b + blob_size;
        const uintptr_t slot = reinterpret_cast<uintptr_t>(&s);
        if (slot < b || slot + sizeof(String) > e)
        {
            return false;
        }
        return string_payload(blob_begin, blob_size, s.data);
    }

    template <typename T> static bool array(const std::byte* blob_begin, size_t blob_size, const Array<T>& self) noexcept
    {
        const uintptr_t b = reinterpret_cast<uintptr_t>(blob_begin);
        const uintptr_t e = b + blob_size;
        const uintptr_t th = reinterpret_cast<uintptr_t>(&self);
        if (th < b || th + sizeof(Array<T>) > e)
        {
            return false;
        }
        const uint32_t n = self.numElements;
        const roffset_t ro = self.relativeOffset;
        if (n == 0)
        {
            return ro == 0;
        }
        if (ro == 0)
        {
            return false;
        }
        if (sizeof(T) != 0 && size_t(n) > (SIZE_MAX / sizeof(T)))
        {
            return false;
        }
        uintptr_t dataAddr = 0;
        if (!detail::self_rel_target_address(th, ro, &dataAddr))
        {
            return false;
        }
        if (dataAddr < b || (dataAddr % alignof(T)) != 0)
        {
            return false;
        }
        const size_t nbytes = size_t(n) * sizeof(T);
        if (dataAddr + nbytes > e)
        {
            return false;
        }
        const T* data = self.getConstData();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (!field_dispatch(blob_begin, blob_size, data[i]))
            {
                return false;
            }
        }
        return true;
    }

    template <typename Key> static bool hashset(const std::byte* blob_begin, size_t blob_size, const HashSet<Key>& hs) noexcept
    {
        return hash_chain_graph_validate(
            blob_begin, blob_size, hs,
            [blob_begin, blob_size](const typename HashSet<Key>::Node& n) noexcept -> bool {
                return field_dispatch(blob_begin, blob_size, n.key);
            });
    }

    template <typename Key, typename Value> static bool hashmap(const std::byte* blob_begin, size_t blob_size, const HashMap<Key, Value>& hm) noexcept
    {
        return hash_chain_graph_validate(
            blob_begin, blob_size, hm,
            [blob_begin, blob_size](const typename HashMap<Key, Value>::Node& n) noexcept -> bool {
                if (!field_dispatch(blob_begin, blob_size, n.key))
                {
                    return false;
                }
                return field_dispatch(blob_begin, blob_size, n.value);
            });
    }

    template <typename TRoot> static bool shallow_root_in_span(const std::byte* blob_begin, size_t blob_size, const TRoot& root) noexcept
    {
        const uintptr_t b = reinterpret_cast<uintptr_t>(blob_begin);
        const uintptr_t e = b + blob_size;
        const uintptr_t o = reinterpret_cast<uintptr_t>(&root);
        if (o < b || o + sizeof(TRoot) > e)
        {
            return false;
        }
        if ((o % alignof(TRoot)) != 0)
        {
            return false;
        }
        return true;
    }

    template <typename U> static constexpr bool is_direct_zm_container_v = is_zm_pointer<U>::value || is_zm_array<U>::value || is_zm_hashset<U>::value || is_zm_hashmap<U>::value
        || std::is_same<U, String>::value;

    template <typename T> static bool field_dispatch(const std::byte* blob_begin, size_t blob_size, const T& v) noexcept
    {
        using U = std::remove_cv_t<T>;
        if constexpr (std::is_same<U, String>::value)
        {
            return string(blob_begin, blob_size, v);
        }
        else if constexpr (is_zm_pointer<U>::value)
        {
            return pointer(blob_begin, blob_size, v);
        }
        else if constexpr (is_zm_array<U>::value)
        {
            return array(blob_begin, blob_size, v);
        }
        else if constexpr (is_zm_hashset<U>::value)
        {
            return hashset(blob_begin, blob_size, v);
        }
        else if constexpr (is_zm_hashmap<U>::value)
        {
            return hashmap(blob_begin, blob_size, v);
        }
        else if constexpr (std::is_same<U, char>::value || std::is_same<U, unsigned char>::value || std::is_same<U, signed char>::value)
        {
            (void)blob_begin;
            (void)blob_size;
            (void)v;
            return true;
        }
        else if constexpr (std::is_trivially_copyable<U>::value)
        {
            (void)blob_begin;
            (void)blob_size;
            (void)v;
            return true;
        }
        else
        {
            (void)blob_begin;
            (void)blob_size;
            (void)v;
            return false;
        }
    }
};

/*

**Composite `TRoot` deep validation (opt-in)**

`validate_blob_view` only walks nested `zm::` fields when `TRoot` itself is a direct container type.
For struct roots, specialize `blob_root_deep_validate<TRoot>` with `enabled = true` and implement
`validate(...)` using `BlobLayoutValidator::field_dispatch` on each embedded field. Untrusted blobs
with composite roots should use that hook or `validate_blob_view_strict`, which refuses to compile
unless deep validation is wired or the root is a direct container.

*/

template <typename TRoot, typename = void> struct blob_root_deep_validate
{
    static constexpr bool enabled = false;
    static bool validate(const std::byte* /*blob_begin*/, size_t /*blob_size*/, const TRoot& /*root*/) noexcept { return true; }
};

template <typename TRoot>
inline constexpr bool zm_blob_root_validation_complete_v = BlobLayoutValidator::is_direct_zm_container_v<std::remove_cv_t<TRoot>>
    || blob_root_deep_validate<TRoot>::enabled;

template <typename TRoot>
ZMEYA_NODISCARD inline BlobViewError validate_blob_view(const std::byte* bytes, size_t byte_count, uint32_t schema_id = 0) noexcept
{
    (void)schema_id;
    if (byte_count < sizeof(TRoot))
    {
        return BlobViewError::SpanTooSmall;
    }
    if ((reinterpret_cast<uintptr_t>(bytes) % alignof(TRoot)) != 0)
    {
        return BlobViewError::BadAlignment;
    }
    const TRoot& root = *reinterpret_cast<const TRoot*>(bytes);
    using U = std::remove_cv_t<TRoot>;
    if constexpr (BlobLayoutValidator::is_direct_zm_container_v<U>)
    {
        if (!BlobLayoutValidator::field_dispatch(bytes, byte_count, root))
        {
            return BlobViewError::BadNested;
        }
    }
    else
    {
        if (!BlobLayoutValidator::shallow_root_in_span(bytes, byte_count, root))
        {
            return BlobViewError::SpanTooSmall;
        }
        if constexpr (blob_root_deep_validate<TRoot>::enabled)
        {
            if (!blob_root_deep_validate<TRoot>::validate(bytes, byte_count, root))
            {
                return BlobViewError::BadNested;
            }
        }
    }
    return BlobViewError::Ok;
}

template <typename TRoot>
ZMEYA_NODISCARD inline BlobViewError validate_blob_view_strict(const std::byte* bytes, size_t byte_count, uint32_t schema_id = 0) noexcept
{
    static_assert(zm_blob_root_validation_complete_v<TRoot>,
        "validate_blob_view_strict: specialize zm::blob_root_deep_validate<TRoot> (enabled=true and validate()) for composite roots, or use a direct zm container as TRoot");
    return validate_blob_view<TRoot>(bytes, byte_count, schema_id);
}

template <typename TRoot>
ZMEYA_NODISCARD inline const TRoot* as_root_blob(const std::byte* bytes, size_t byte_count, uint32_t schema_id = 0) noexcept
{
    if (validate_blob_view<TRoot>(bytes, byte_count, schema_id) != BlobViewError::Ok)
    {
        return nullptr;
    }
    return reinterpret_cast<const TRoot*>(bytes);
}

template <typename Key>
ZMEYA_NODISCARD inline BlobViewError validate_hashset_in_blob(const std::byte* blob_begin, size_t blob_size, const HashSet<Key>& hs) noexcept
{
    return BlobLayoutValidator::hashset(blob_begin, blob_size, hs) ? BlobViewError::Ok : BlobViewError::BadNested;
}

template <typename Key, typename Value>
ZMEYA_NODISCARD inline BlobViewError validate_hashmap_in_blob(const std::byte* blob_begin, size_t blob_size, const HashMap<Key, Value>& hm) noexcept
{
    return BlobLayoutValidator::hashmap(blob_begin, blob_size, hm) ? BlobViewError::Ok : BlobViewError::BadNested;
}

template <typename T>
ZMEYA_NODISCARD inline BlobViewError validate_array_in_blob(const std::byte* blob_begin, size_t blob_size, const Array<T>& arr) noexcept
{
    return BlobLayoutValidator::array(blob_begin, blob_size, arr) ? BlobViewError::Ok : BlobViewError::BadNested;
}

ZMEYA_NODISCARD inline BlobViewError validate_string_in_blob(const std::byte* blob_begin, size_t blob_size, const String& s) noexcept
{
    return BlobLayoutValidator::string(blob_begin, blob_size, s) ? BlobViewError::Ok : BlobViewError::BadNested;
}

template <typename T>
ZMEYA_NODISCARD inline BlobViewError validate_pointer_in_blob(const std::byte* blob_begin, size_t blob_size, const Pointer<T>& p) noexcept
{
    return BlobLayoutValidator::pointer(blob_begin, blob_size, p) ? BlobViewError::Ok : BlobViewError::BadNested;
}

ZMEYA_NODISCARD inline const char* try_c_str_in_blob(const String& s, const std::byte* blob_begin, size_t blob_size) noexcept
{
    if (!BlobLayoutValidator::string(blob_begin, blob_size, s))
    {
        return nullptr;
    }
    return s.c_str();
}

template <typename TRoot>
ZMEYA_NODISCARD inline const TRoot* as_root_blob_strict(const std::byte* bytes, size_t byte_count, uint32_t schema_id = 0) noexcept
{
    if (validate_blob_view_strict<TRoot>(bytes, byte_count, schema_id) != BlobViewError::Ok)
    {
        return nullptr;
    }
    return reinterpret_cast<const TRoot*>(bytes);
}

} // namespace zm
