#pragma once

#include "ZmeyaString.h"
#include <optional>

namespace zm
{

/*

**Self-relative array**

The header stores count plus an offset to the first element. Incremental mutation APIs exist only
when `ZMEYA_ENABLE_SERIALIZE_SUPPORT` is enabled.

**Invalidation (same idea as `std::vector`)**

During `write_blob` / `BlobWriter`, do not cache raw pointers or iterators into this array's elements
across `push_back` / `resize` / `erase_at` / similar on the same array. Use `w.root()->...` each time.
After finalize, a `const` view over the returned blob is stable for that buffer.

*/

template <typename T> class Array
{
    roffset_t relativeOffset;
    uint32_t numElements;

    friend struct BlobLayoutValidator;

  private:
    ZMEYA_NODISCARD const T* getConstData() const noexcept
    {
        uintptr_t addr = toAbsoluteAddr(uintptr_t(this), relativeOffset);
        return reinterpret_cast<const T*>(addr);
    }

    ZMEYA_NODISCARD T* getData() const noexcept { return const_cast<T*>(getConstData()); }

  public:
    Array() noexcept = default;

    ZMEYA_NODISCARD size_t size() const noexcept { return size_t(numElements); }

    ZMEYA_NODISCARD const T& operator[](const size_t index) const noexcept
    {
        const T* data = getConstData();
        return data[index];
    }

    ZMEYA_NODISCARD std::optional<std::reference_wrapper<const T>> try_at(const size_t index) const noexcept
    {
        if (index >= size())
        {
            return std::nullopt;
        }
        const T* data = getConstData();
        return std::cref(data[index]);
    }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT

    ZMEYA_NODISCARD T* get_element_ptr_unsafe_can_be_relocated(const size_t index) noexcept
    {
        T* data = getData();
        return data + index;
    }

    ZMEYA_NODISCARD T* get_raw_ptr_unsafe_can_be_relocated() noexcept { return getData(); }
#endif

    ZMEYA_NODISCARD const T* at(const size_t index) const
    {
        ZMEYA_ASSERT(index < size());
        const T* p = getConstData();
        return p + index;
    }

    ZMEYA_NODISCARD const T* data() const noexcept { return getConstData(); }

    ZMEYA_NODISCARD const T* begin() const noexcept
    {
        const T* data = getConstData();
        return data;
    };

    ZMEYA_NODISCARD const T* end() const noexcept
    {
        const T* data = getConstData();
        return data + size();
    };

    ZMEYA_NODISCARD bool empty() const noexcept { return size() == 0; }

#ifdef ZMEYA_ENABLE_SERIALIZE_SUPPORT
    template <typename U> void push_back(U&& v);
    void pop_back();
    void clear();
    void erase_at(size_t index);
    void resize(size_t new_size, const T& fill = T{});
#endif

    template <typename F> Array<T>& operator=(const std::vector<F>& other)
    {
        assign(*this, other);
        return *this;
    }

    template <typename T, typename F> friend void assign(Array<T>& to, const std::vector<F>& from);
    template <typename Key, typename F> friend void assign(HashSet<Key>& to, const std::unordered_set<F>& from);
    template <typename Key, typename Value, typename FK, typename FV>
    friend void assign(HashMap<Key, Value>& to, const std::unordered_map<FK, FV>& from);
    template <typename T> friend void assign(Array<zm::Pointer<T>>& to, const std::vector<T*>& from);
    friend class detail::BuilderBase;
};

namespace detail
{

/*

**`array_push_back` element guard**

Slab growth uses `memcpy` relocation; elements must not embed self-relative edges or non-trivial state.

*/

template <typename U> struct zm_array_push_back_ok : std::integral_constant<bool, std::is_trivially_copyable<U>::value>
{
};
template <> struct zm_array_push_back_ok<String> : std::false_type
{
};
template <typename P> struct zm_array_push_back_ok<Pointer<P>> : std::false_type
{
};
template <typename A> struct zm_array_push_back_ok<Array<A>> : std::false_type
{
};
} // namespace detail

} // namespace zm
