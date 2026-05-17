#include "Zmeya.h"
#include <gtest/gtest.h>
#include <cstring>
#include <unordered_set>
#include <vector>

struct HashSetIntRoot
{
    zm::HashSet<int> h;
};

struct IntArrayRoot
{
    zm::Array<int> ints;
};

struct IntPtrRoot
{
    zm::Pointer<int> p;
};

namespace zm
{

template <> struct blob_root_deep_validate<HashSetIntRoot>
{
    static constexpr bool enabled = true;
    static bool validate(const std::byte* blob_begin, size_t blob_size, const HashSetIntRoot& root) noexcept
    {
        return BlobLayoutValidator::field_dispatch(blob_begin, blob_size, root.h);
    }
};

template <> struct blob_root_deep_validate<IntArrayRoot>
{
    static constexpr bool enabled = true;
    static bool validate(const std::byte* blob_begin, size_t blob_size, const IntArrayRoot& root) noexcept
    {
        return BlobLayoutValidator::field_dispatch(blob_begin, blob_size, root.ints);
    }
};

template <> struct blob_root_deep_validate<IntPtrRoot>
{
    static constexpr bool enabled = true;
    static bool validate(const std::byte* blob_begin, size_t blob_size, const IntPtrRoot& root) noexcept
    {
        return BlobLayoutValidator::field_dispatch(blob_begin, blob_size, root.p);
    }
};

} // namespace zm

TEST(MemorySafety, MurmurOddAddressDoesNotFault)
{
    alignas(8) char buf[16];
    std::memset(buf, 'A', sizeof(buf));
    const char* mis = buf + 1;
    const uint32_t len = 7;
    const uint64_t h = zm::murmur_hash_process64a(mis, len, 42);
    EXPECT_NE(h, 0u);
}

TEST(MemorySafety, HashCStringBoundedNoNul)
{
    char buf[4] = {'x', 'y', 'z', 'q'};
    const size_t h = zm::HashUtils::hashCStringBounded(buf, 3);
    const size_t h2 = zm::HashUtils::hashStringBytes(buf, 3);
    EXPECT_EQ(h, h2);
}

TEST(MemorySafety, ValidateHashSetDeepRoundTrip)
{
    zm::BlobBuffer blob = zm::write_scope<HashSetIntRoot>([](zm::BlobWriter<HashSetIntRoot>& w) {
        zm::assign(w.root()->h, std::unordered_set<int>{5, 9, 1});
    });
    const std::byte* bytes = reinterpret_cast<const std::byte*>(blob.data());
    const HashSetIntRoot* root = reinterpret_cast<const HashSetIntRoot*>(blob.data());
    EXPECT_EQ(zm::validate_hashset_in_blob(bytes, blob.size(), root->h), zm::BlobViewError::Ok);
    EXPECT_EQ(zm::validate_blob_view<HashSetIntRoot>(bytes, blob.size()), zm::BlobViewError::Ok);
    EXPECT_EQ(zm::validate_blob_view_strict<HashSetIntRoot>(bytes, blob.size()), zm::BlobViewError::Ok);
    EXPECT_NE(zm::as_root_blob_strict<HashSetIntRoot>(bytes, blob.size()), nullptr);
}

TEST(MemorySafety, ValidateBlobViewShallowTooSmall)
{
    zm::BlobBuffer blob = zm::write_scope<HashSetIntRoot>([](zm::BlobWriter<HashSetIntRoot>& w) {
        zm::assign(w.root()->h, std::unordered_set<int>{2});
    });
    const std::byte* bytes = reinterpret_cast<const std::byte*>(blob.data());
    EXPECT_NE(zm::validate_blob_view<HashSetIntRoot>(bytes, sizeof(HashSetIntRoot) - 1), zm::BlobViewError::Ok);
}

TEST(MemorySafety, ArrayTryAtBounds)
{
    zm::BlobBuffer blob = zm::write_scope<IntArrayRoot>([](zm::BlobWriter<IntArrayRoot>& w) {
        zm::assign(w.root()->ints, std::vector<int>{10, 20});
    });
    const IntArrayRoot* root = reinterpret_cast<const IntArrayRoot*>(blob.data());
    ASSERT_TRUE(root->ints.try_at(0).has_value());
    EXPECT_EQ(root->ints.try_at(0)->get(), 10);
    EXPECT_FALSE(root->ints.try_at(2).has_value());
}

TEST(MemorySafety, PointerTryGetInBlob)
{
    zm::BlobBuffer blob = zm::write_scope<IntPtrRoot>([](zm::BlobWriter<IntPtrRoot>& w) {
        zm::ArenaRef<int> slot = w.allocate<int>();
        *slot = 12345;
        zm::assign(w.root()->p, slot.transient_ptr());
    });
    const IntPtrRoot* root = reinterpret_cast<const IntPtrRoot*>(blob.data());
    const std::byte* b = reinterpret_cast<const std::byte*>(blob.data());
    int* got = root->p.try_get_in_blob(b, blob.size());
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(*got, 12345);
}

TEST(MemorySafety, GarbageHashSetFailsDeepValidate)
{
    alignas(zm::HashSet<int>) unsigned char raw[sizeof(zm::HashSet<int>)];
    std::memset(raw, 0xff, sizeof(raw));
    const zm::HashSet<int>& hs = *reinterpret_cast<const zm::HashSet<int>*>(raw);
    const std::byte* bytes = reinterpret_cast<const std::byte*>(raw);
    EXPECT_NE(zm::validate_hashset_in_blob(bytes, sizeof(raw), hs), zm::BlobViewError::Ok);
}

TEST(MemorySafety, SelfRelativeAddressOverflowRejected)
{
    uintptr_t out = 0;
    EXPECT_FALSE(zm::detail::self_rel_target_address(static_cast<uintptr_t>(-5), 10, &out));
}

TEST(MemorySafety, TryCStrInBlobNullWhenInvalid)
{
    alignas(zm::String) unsigned char raw[sizeof(zm::String)];
    std::memset(raw, 0xff, sizeof(raw));
    const zm::String& s = *reinterpret_cast<const zm::String*>(raw);
    const std::byte* b = reinterpret_cast<const std::byte*>(raw);
    EXPECT_EQ(zm::try_c_str_in_blob(s, b, sizeof(raw)), nullptr);
}
