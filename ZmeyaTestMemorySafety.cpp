#include "Zmeya.h"
#include <gtest/gtest.h>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace
{

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

struct HashSetIntRoot
{
    zm::HashSet<int> h;
};

TEST(MemorySafety, ValidateHashSetDeepRoundTrip)
{
    zm::BlobBuffer blob = zm::write_scope<HashSetIntRoot>([](zm::BlobWriter<HashSetIntRoot>& w) {
        zm::assign(w.root()->h, std::unordered_set<int>{5, 9, 1});
    });
    const std::byte* bytes = reinterpret_cast<const std::byte*>(blob.data());
    const HashSetIntRoot* root = reinterpret_cast<const HashSetIntRoot*>(blob.data());
    EXPECT_EQ(zm::validate_hashset_in_blob(bytes, blob.size(), root->h), zm::BlobViewError::Ok);
    EXPECT_EQ(zm::validate_blob_view<HashSetIntRoot>(bytes, blob.size()), zm::BlobViewError::Ok);
}

TEST(MemorySafety, ValidateBlobViewShallowTooSmall)
{
    zm::BlobBuffer blob = zm::write_scope<HashSetIntRoot>([](zm::BlobWriter<HashSetIntRoot>& w) {
        zm::assign(w.root()->h, std::unordered_set<int>{2});
    });
    const std::byte* bytes = reinterpret_cast<const std::byte*>(blob.data());
    EXPECT_NE(zm::validate_blob_view<HashSetIntRoot>(bytes, sizeof(HashSetIntRoot) - 1), zm::BlobViewError::Ok);
}

struct IntArrayRoot
{
    zm::Array<int> ints;
};

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

struct IntPtrRoot
{
    zm::Pointer<int> p;
};

TEST(MemorySafety, PointerTryGetInBlob)
{
    zm::BlobBuffer blob = zm::write_scope<IntPtrRoot>([](zm::BlobWriter<IntPtrRoot>& w) {
        int* slot = w.allocate<int>();
        *slot = 12345;
        zm::assign(w.root()->p, slot);
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

} // namespace
