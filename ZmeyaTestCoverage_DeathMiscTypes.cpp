#include "ZmeyaTestCoverageCommon.h"

// P10-01: assign(String, ...) outside an active builder hits ZMEYA_ASSERT (death test).
#if GTEST_HAS_DEATH_TEST
TEST(ZmeyaDeathTestSuite, Coverage_P10_AssignStringOutsideWriteBlobAborts)
{
    zm::String s;
    EXPECT_DEATH({ zm::assign(s, std::string("x")); }, ".*");
}
#endif

// P10-02: string_append with a null suffix pointer aborts before strlen runs on invalid memory.
#if GTEST_HAS_DEATH_TEST
TEST(ZmeyaDeathTestSuite, Coverage_P10_StringAppendNullptrAborts)
{
    EXPECT_DEATH(
        {
            (void)zm::write_scope<CovStringRoot>(
                [](zm::BlobWriter<CovStringRoot>& w)
                {
                    w.string_append(w.root()->text, nullptr);
                });
        },
        ".*");
}
#endif

// P10-03: finalize(0) is rejected (invalid alignment).
#if GTEST_HAS_DEATH_TEST
TEST(ZmeyaDeathTestSuite, Coverage_P10_FinalizeZeroAlignmentAborts)
{
    EXPECT_DEATH(
        {
            std::unique_ptr<zm::detail::Builder<CovStringRoot>> b = zm::detail::Builder<CovStringRoot>::create(64);
            b->finalize(0);
        },
        ".*");
}
#endif

// P11-01: One root touches strings, arrays, hash containers, pointers, and nested arrays together.
TEST(ZmeyaTestSuite, Coverage_P11_SingleRootAllMajorKinds)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovAllKindsRoot>(
        [](zm::BlobWriter<CovAllKindsRoot>& w)
        {
            w.root()->s = std::string("all_kinds");
            w.array_push_back(w.root()->ints, 7);
            w.hashset_insert(w.root()->hs, 9);
            w.hashmap_insert(w.root()->hm, std::string("k"), 3);
            std::vector<std::vector<int32_t>> g = {{1, 2}, {3}};
            w.root()->grid = g;
            zm::ArenaRef<CovPointerChainNode> n = w.allocate<CovPointerChainNode>();
            n->id = 99;
            n->next = nullptr;
            w.root()->node = n.transient_ptr();
        }, 512, 8);

    const CovAllKindsRoot* rr = reinterpret_cast<const CovAllKindsRoot*>(blob.data());
    EXPECT_EQ(rr->s, std::string("all_kinds"));
    ASSERT_EQ(rr->ints.size(), 1u);
    EXPECT_EQ(rr->ints[0], 7);
    EXPECT_TRUE(rr->hs.contains(9));
    EXPECT_EQ(rr->hm.find("k", -1), 3);
    ASSERT_EQ(rr->grid.size(), 2u);
    ASSERT_NE(rr->node.get(), nullptr);
    EXPECT_EQ(rr->node->id, 99);
}

// P11-02: Mixed incremental array growth and bulk string/hash assign in one session.
TEST(ZmeyaTestSuite, Coverage_P11_MixedIncrementalAndBulkInOneBlob)
{
    struct Root
    {
        zm::String s;
        zm::Array<int32_t> a;
        zm::HashMap<zm::String, int32_t> m;
    };

    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<Root>(
        [](zm::BlobWriter<Root>& w)
        {
            for (int i = 0; i < 20; ++i)
            {
                w.array_push_back(w.root()->a, i);
            }
            w.root()->s = std::string("bulk_after_incremental");
            w.hashmap_insert(w.root()->m, std::string("x"), 1);
        }, 128, 4);

    const Root* rr = reinterpret_cast<const Root*>(blob.data());
    EXPECT_EQ(rr->a.size(), 20u);
    EXPECT_EQ(rr->s, std::string("bulk_after_incremental"));
    EXPECT_EQ(rr->m.find("x", -1), 1);
}

// P11-03: Forward-compatible roots can carry a version field next to magic for future readers (numeric round-trip only).
TEST(ZmeyaTestSuite, Coverage_P11_VersionFieldRoundTrip)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovVersionRoot>(
        [](zm::BlobWriter<CovVersionRoot>& w)
        {
            w.root()->magic = 0x59454D5Au;
            w.root()->version = 3u;
            w.root()->payload = -77;
        }, 128, 4);

    const CovVersionRoot* r = reinterpret_cast<const CovVersionRoot*>(blob.data());
    EXPECT_EQ(r->magic, 0x59454D5Au);
    EXPECT_EQ(r->version, 3u);
    EXPECT_EQ(r->payload, -77);
}

// P12-01: Two identical write_scope sessions for a POD-only root can yield byte-identical blobs (deterministic layout).
TEST(ZmeyaTestSuite, Coverage_P12_TwoWriteBlobPODDeterministicBytes)
{
    auto make = []()
    {
        return zmeya_test::write_scope_stressed<CovIntArrayRoot>(
            [](zm::BlobWriter<CovIntArrayRoot>& w)
            {
                for (int i = 0; i < 8; ++i)
                {
                    w.array_push_back(w.root()->values, int32_t(i * i));
                }
            }, 1024, 8);
    };
    zm::BlobBuffer a = make();
    zm::BlobBuffer b = make();
    ASSERT_EQ(a.size(), b.size());
    EXPECT_EQ(std::memcmp(a.data(), b.data(), a.size()), 0);
}

// P13-01: Release-only wall time bound for incremental hash inserts (guards catastrophic regressions).
#ifndef NDEBUG
TEST(ZmeyaTestSuite, Coverage_P13_IncrementalHashWallTimeBoundReleaseOnly)
{
    GTEST_SKIP() << "timing bound is checked in Release builds only";
}
#else
TEST(ZmeyaTestSuite, Coverage_P13_IncrementalHashWallTimeBoundReleaseOnly)
{
    std::unordered_map<int32_t, int32_t> model;
    for (int i = 0; i < 8000; ++i)
    {
        model[i] = i * 2;
    }
    auto t0 = std::chrono::steady_clock::now();
    zm::BlobBuffer blob = zm::write_scope<CovHashMapIntIntRoot>(
        [&model](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            for (const auto& kv : model)
            {
                w.hashmap_insert(w.root()->map, kv.first, kv.second);
            }
        }, 4);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(ms, 5000) << "incremental hash rebuild should stay well under a few seconds at this N";
    EXPECT_FALSE(blob.empty());
}
#endif

// P14-01: zm::Pair fields round-trip as plain members on the read side.
TEST(ZmeyaTestSuite, Coverage_P14_PairFieldRoundTrip)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovPairRoot>(
        [](zm::BlobWriter<CovPairRoot>& w)
        {
            w.root()->p.first = -9;
            w.root()->p.second = 2.5f;
        }, 64, 4);

    const CovPairRoot* r = reinterpret_cast<const CovPairRoot*>(blob.data());
    EXPECT_EQ(r->p.first, -9);
    EXPECT_FLOAT_EQ(r->p.second, 2.5f);
}

// P14-02: enum class stored as uint32_t field round-trips through write_scope.
TEST(ZmeyaTestSuite, Coverage_P14_EnumClassFieldRoundTrip)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovEnumRoot>(
        [](zm::BlobWriter<CovEnumRoot>& w)
        {
            w.root()->e = CovEnumField::B;
        }, 64, 4);

    EXPECT_EQ(reinterpret_cast<const CovEnumRoot*>(blob.data())->e, CovEnumField::B);
}
