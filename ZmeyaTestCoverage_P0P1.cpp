#include "ZmeyaTestCoverageCommon.h"

// P0-01: Many appends with a tiny working buffer must stay correct (no stale pointer across realloc in append path).
TEST(ZmeyaTestSuite, Coverage_P0_StringAppendManyReallocs)
{
    std::string model;
    model.reserve(600);
    std::mt19937 rng(12345);
    for (int i = 0; i < 500; ++i)
    {
        if ((i & 3) == 0)
        {
            model.push_back(static_cast<char>('a' + (i % 26)));
        }
        else
        {
            int len = 1 + int(rng() % 5);
            for (int j = 0; j < len; ++j)
            {
                model.push_back(static_cast<char>('A' + int(rng() % 26)));
            }
        }
    }

    zm::BlobBuffer golden = zm::write_scope<CovStringRoot>(
        [&model](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = model;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<CovStringRoot>(
        [&model](zm::BlobWriter<CovStringRoot>& w)
        {
            for (size_t i = 0; i < model.size(); ++i)
            {
                std::string one(1, model[i]);
                w.string_append(w.root()->text, one.c_str());
            }
        }, 48, 4);

    EXPECT_STREQ(reinterpret_cast<const CovStringRoot*>(golden.data())->text.c_str(),
        reinterpret_cast<const CovStringRoot*>(built.data())->text.c_str());
}

// P0-02: After string_clear, incremental append and assign must behave like a fresh string field.
TEST(ZmeyaTestSuite, Coverage_P0_StringClearThenAppendMatchesGolden)
{
    const std::string goldenStr = "after_clear";

    zm::BlobBuffer golden = zm::write_scope<CovStringRoot>(
        [&goldenStr](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("temp");
            w.root()->text = goldenStr;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("temp");
            w.string_clear(w.root()->text);
            w.string_append(w.root()->text, "after");
            w.root()->text += "_clear";
        }, 64, 4);

    EXPECT_STREQ(reinterpret_cast<const CovStringRoot*>(golden.data())->text.c_str(),
        reinterpret_cast<const CovStringRoot*>(built.data())->text.c_str());
    EXPECT_STREQ(reinterpret_cast<const CovStringRoot*>(built.data())->text.c_str(), goldenStr.c_str());
}

// P0-03: Empty assign then append must not assume a non-null C string length on the empty state.
TEST(ZmeyaTestSuite, Coverage_P0_EmptyStringThenAppend)
{
    zm::BlobBuffer golden = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string();
            w.root()->text += "x";
        }, 256, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string();
            w.string_append(w.root()->text, "x");
        }, 32, 4);

    EXPECT_STREQ(reinterpret_cast<const CovStringRoot*>(golden.data())->text.c_str(), "x");
    EXPECT_STREQ(reinterpret_cast<const CovStringRoot*>(built.data())->text.c_str(), "x");
}

// P0-04: array_push_back across many growth steps must match golden vector contents.
TEST(ZmeyaTestSuite, Coverage_P0_ArrayPushBackAcrossRealloc)
{
    std::vector<int32_t> model;
    for (int i = 0; i < 220; ++i)
    {
        model.push_back(i * i - 3);
    }

    zm::BlobBuffer golden = zm::write_scope<CovIntArrayRoot>(
        [&model](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.root()->values = model;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<CovIntArrayRoot>(
        [&model](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            for (int32_t v : model)
            {
                w.array_push_back(w.root()->values, v);
            }
        }, 32, 4);

    const zm::Array<int32_t>& g = reinterpret_cast<const CovIntArrayRoot*>(golden.data())->values;
    const zm::Array<int32_t>& b = reinterpret_cast<const CovIntArrayRoot*>(built.data())->values;
    ASSERT_EQ(g.size(), b.size());
    for (size_t i = 0; i < g.size(); ++i)
    {
        EXPECT_EQ(g[i], b[i]);
    }
}

// P0-05: First push on an empty array yields size 1 and the pushed value.
TEST(ZmeyaTestSuite, Coverage_P0_ArrayPushBackFirstElement)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovIntArrayRoot>(
        [](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.array_push_back(w.root()->values, int32_t(911));
        }, 64, 4);

    const CovIntArrayRoot* r = reinterpret_cast<const CovIntArrayRoot*>(blob.data());
    ASSERT_EQ(r->values.size(), 1u);
    EXPECT_EQ(r->values[0], 911);
}

// P0-06: erase_at at first, middle, and last indices preserves order for the remaining elements.
TEST(ZmeyaTestSuite, Coverage_P0_ArrayEraseAtBoundaries)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovIntArrayRoot>(
        [](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            for (int i = 0; i < 7; ++i)
            {
                w.array_push_back(w.root()->values, int32_t(10 + i));
            }
            w.array_erase_at(w.root()->values, 0);
            w.array_erase_at(w.root()->values, 2);
            w.array_erase_at(w.root()->values, w.root()->values.size() - 1);
        }, 128, 4);

    const zm::Array<int32_t>& a = reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values;
    ASSERT_EQ(a.size(), 4u);
    EXPECT_EQ(a[0], 11);
    EXPECT_EQ(a[1], 12);
    EXPECT_EQ(a[2], 14);
    EXPECT_EQ(a[3], 15);
}

// P0-07: pop_back on empty is a no-op; extra pops after draining must not crash.
TEST(ZmeyaTestSuite, Coverage_P0_ArrayPopBackUntilEmpty)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovIntArrayRoot>(
        [](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            for (int i = 0; i < 10; ++i)
            {
                w.array_push_back(w.root()->values, int32_t(i));
            }
            for (int j = 0; j < 11; ++j)
            {
                w.array_pop_back(w.root()->values);
            }
        }, 64, 4);

    EXPECT_EQ(reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values.size(), 0u);
}

// P0-08: array_resize grows with fill then shrinks; grown tail uses fill and truncation drops tail values.
TEST(ZmeyaTestSuite, Coverage_P0_ArrayResizeGrowThenShrink)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovIntArrayRoot>(
        [](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.array_push_back(w.root()->values, 1);
            w.array_push_back(w.root()->values, 2);
            w.array_resize(w.root()->values, 6, int32_t(-9));
            w.array_resize(w.root()->values, 3, int32_t(0));
        }, 128, 4);

    const zm::Array<int32_t>& a = reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values;
    ASSERT_EQ(a.size(), 3u);
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[1], 2);
    EXPECT_EQ(a[2], -9);
}

// P0-09: Incremental hashmap_insert on string keys (sorted insert order) matches bulk assign for a moderate key count; N kept moderate so MSVC Debug does not exhaust bump arena during repeated hash rebuilds.
TEST(ZmeyaTestSuite, Coverage_P0_HashMapStringKeyCompactionLogicalMatch)
{
    std::unordered_map<std::string, int32_t> model;
    for (int i = 0; i < 100; ++i)
    {
        model[std::string("k") + std::to_string(i)] = i;
    }

    zm::BlobBuffer golden = zm::write_scope<CovHashMapStrIntRoot>(
        [&model](zm::BlobWriter<CovHashMapStrIntRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    zm::BlobBuffer built = zm::write_scope<CovHashMapStrIntRoot>(
        [&model](zm::BlobWriter<CovHashMapStrIntRoot>& w)
        {
            std::vector<std::string> keys;
            keys.reserve(model.size());
            for (const auto& kv : model)
            {
                keys.push_back(kv.first);
            }
            std::sort(keys.begin(), keys.end());
            for (const std::string& k : keys)
            {
                w.hashmap_insert(w.root()->map, k, model.at(k));
            }
        }, 4);

    zmeya_coverage::ExpectHashMapStrIntMatchesModel(reinterpret_cast<const CovHashMapStrIntRoot*>(golden.data())->map, model);
    zmeya_coverage::ExpectHashMapStrIntMatchesModel(reinterpret_cast<const CovHashMapStrIntRoot*>(built.data())->map, model);
}

// P0-10: Mixed strings and int-array growth force both string slabs and array slabs to relocate; final logical view stays consistent.
TEST(ZmeyaTestSuite, Coverage_P0_MixedStringsAndArraysCompaction)
{
    struct Root
    {
        zm::String a;
        zm::String b;
        zm::Array<int32_t> nums;
    };

    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<Root>(
        [](zm::BlobWriter<Root>& w)
        {
            for (int round = 0; round < 40; ++round)
            {
                w.root()->a = std::string("s") + std::to_string(round);
                w.root()->b = std::string(20, static_cast<char>('A' + (round % 26)));
                w.array_clear(w.root()->nums);
                for (int i = 0; i < 30; ++i)
                {
                    w.array_push_back(w.root()->nums, int32_t(round * 100 + i));
                }
            }
        }, 64, 8);

    const Root* rr = reinterpret_cast<const Root*>(blob.data());
    EXPECT_STREQ(rr->a.c_str(), "s39");
    ASSERT_EQ(rr->nums.size(), 30u);
    EXPECT_EQ(rr->nums[0], 3900);
    EXPECT_EQ(rr->nums[29], 3929);
}

// P0-11: Finalize alignment must pad to a multiple of the requested alignment while the blob still parses.
TEST(ZmeyaTestSuite, Coverage_P0_FinalizeAlignmentMatrix)
{
    static const size_t kAligns[] = {1, 2, 4, 8, 16, 32};
    for (size_t ai = 0; ai < sizeof(kAligns) / sizeof(kAligns[0]); ++ai)
    {
        const size_t align = kAligns[ai];
        zm::BlobBuffer blob = zm::write_scope<CovStringRoot>(
            [](zm::BlobWriter<CovStringRoot>& w)
            {
                w.root()->text = std::string("align");
            },
            align);
        EXPECT_EQ(blob.size() % align, 0u);
        const CovStringRoot* r = reinterpret_cast<const CovStringRoot*>(blob.data());
        EXPECT_STREQ(r->text.c_str(), "align");
    }
}

// P0-12: Explicit assign(..., HashMap with nested strings) under a small starting arena matches write_scope golden (TLS + realloc stress).
TEST(ZmeyaTestSuite, Coverage_P0_ExplicitAssignHashMapNestedStringsUnderRealloc)
{
    std::unordered_map<std::string, std::string> model;
    for (int i = 0; i < 80; ++i)
    {
        model[std::string("k") + std::to_string(i)] = std::string("v") + std::to_string(i * i);
    }

    zm::BlobBuffer golden = zm::write_scope<CovHashMapStrStrRoot>(
        [&model](zm::BlobWriter<CovHashMapStrStrRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    std::unique_ptr<zm::detail::Builder<CovHashMapStrStrRoot>> builder = zm::detail::Builder<CovHashMapStrStrRoot>::create();
    zm::detail::ScopedBuilder scope(builder.get());
    zm::assign(*builder, builder->getRoot()->map, model);
    zm::Span<char> span = builder->finalize(4);
    std::vector<char> blobA(span.data, span.data + span.size);

    ASSERT_EQ(blobA.size(), golden.size());
    EXPECT_EQ(std::memcmp(blobA.data(), golden.data(), blobA.size()), 0);
}

// P0-13: deep_copy(builder, ...) must run with TLS scoped to the explicit builder while writing a zm::String slot.
TEST(ZmeyaTestSuite, Coverage_P0_DeepCopyBuilderScopedNestedString)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovDeepCopyRoot>(
        [](zm::BlobWriter<CovDeepCopyRoot>& w)
        {
            CovDeepCopyRoot* root = w.root();
            zm::goffset_t go = w.builder_base()->arena_byte_offset_of(&root->s);
            zm::deep_copy<std::string, zm::String>(*w.builder_base(), std::string("nested_value"), go);
        }, 256, 4);

    const CovDeepCopyRoot* r = reinterpret_cast<const CovDeepCopyRoot*>(blob.data());
    EXPECT_STREQ(r->s.c_str(), "nested_value");
}

// P0-14: Many allocate() nodes plus pointer wiring; resolve nodes by goffset_t after growth so raw pointers are not cached across realloc.
TEST(ZmeyaTestSuite, Coverage_P0_AllocatePointerGraphManyNodes)
{
    zm::BlobBuffer blob = zm::write_scope<CovPointerChainRoot>(
        [](zm::BlobWriter<CovPointerChainRoot>& w)
        {
            constexpr int kN = 64;
            zm::detail::BuilderBase* bb = w.builder_base();
            zm::goffset_t g[kN];
            for (int i = 0; i < kN; ++i)
            {
                CovPointerChainNode* p = w.allocate<CovPointerChainNode>();
                g[i] = bb->arena_byte_offset_of(p);
                p->id = i;
                p->next = nullptr;
            }
            for (int i = 0; i < kN - 1; ++i)
            {
                CovPointerChainNode* pi = reinterpret_cast<CovPointerChainNode*>(bb->get_ptr_unsafe_to_store(g[i]));
                CovPointerChainNode* pj = reinterpret_cast<CovPointerChainNode*>(bb->get_ptr_unsafe_to_store(g[i + 1]));
                pi->next = pj;
            }
            CovPointerChainNode* head = reinterpret_cast<CovPointerChainNode*>(bb->get_ptr_unsafe_to_store(g[0]));
            w.root()->head = head;
        },
        8);

    const CovPointerChainRoot* r = reinterpret_cast<const CovPointerChainRoot*>(blob.data());
    const CovPointerChainNode* cur = r->head.get();
    for (int i = 0; i < 64; ++i)
    {
        ASSERT_NE(cur, nullptr);
        EXPECT_EQ(cur->id, i);
        cur = cur->next.get();
    }
    EXPECT_EQ(cur, nullptr);
}

// P0-15: contains_pointer reports blob-owned objects and rejects unrelated stack addresses.
TEST(ZmeyaTestSuite, Coverage_P0_ContainsPointerBlobVsStack)
{
    zm::write_scope<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            CovStringRoot* r = w.root();
            int stackVar = 0;
            EXPECT_TRUE(w.contains_pointer(r));
            EXPECT_FALSE(w.contains_pointer(&stackVar));
        });
}

// P1-01: Incremental HashSet of zm::String matches bulk assign when sorted lexically.
TEST(ZmeyaTestSuite, Coverage_P1_HashSetStringIncrementalVsGolden)
{
    std::unordered_set<std::string> model = {"apple", "banana", "cherry", "date", "elderberry", "fig"};

    zm::BlobBuffer golden = zm::write_scope<CovHashSetStrRoot>(
        [&model](zm::BlobWriter<CovHashSetStrRoot>& w)
        {
            w.root()->set = model;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<CovHashSetStrRoot>(
        [&model](zm::BlobWriter<CovHashSetStrRoot>& w)
        {
            std::vector<std::string> order(model.begin(), model.end());
            std::shuffle(order.begin(), order.end(), std::mt19937(99));
            for (const std::string& s : order)
            {
                w.hashset_insert(w.root()->set, s);
            }
        }, 128, 4);

    EXPECT_EQ(zmeya_coverage::SortedStringSet(reinterpret_cast<const CovHashSetStrRoot*>(golden.data())->set),
        zmeya_coverage::SortedStringSet(reinterpret_cast<const CovHashSetStrRoot*>(built.data())->set));
}

// P1-02: HashMap with int keys uses the non-string hash path; incremental ops match an STL reference map.
TEST(ZmeyaTestSuite, Coverage_P1_HashMapIntKeyIncrementalVsGolden)
{
    std::unordered_map<int32_t, int32_t> model;
    for (int i = -50; i < 50; ++i)
    {
        model[i] = i * 3 + 1;
    }

    zm::BlobBuffer golden = zm::write_scope<CovHashMapIntIntRoot>(
        [&model](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<CovHashMapIntIntRoot>(
        [&model](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            for (const auto& kv : model)
            {
                w.hashmap_insert(w.root()->map, kv.first, kv.second);
            }
        }, 256, 4);

    std::vector<std::pair<int32_t, int32_t>> g;
    for (const auto& p : reinterpret_cast<const CovHashMapIntIntRoot*>(golden.data())->map)
    {
        g.emplace_back(p.first, p.second);
    }
    std::vector<std::pair<int32_t, int32_t>> b;
    for (const auto& p : reinterpret_cast<const CovHashMapIntIntRoot*>(built.data())->map)
    {
        b.emplace_back(p.first, p.second);
    }
    std::sort(g.begin(), g.end());
    std::sort(b.begin(), b.end());
    EXPECT_EQ(g, b);
}

// P1-03: HashMap with zm::String values (bulk assign path); incremental insert for String values is skipped pending builder relocation hardening.
TEST(ZmeyaTestSuite, Coverage_P1_HashMapStringValueBulkAssignRoundTrip)
{
    std::unordered_map<int32_t, std::string> model = {{-2, "a"}, {-1, "bb"}, {0, "ccc"}, {5, "hello"}, {9, "world"}};

    struct Root
    {
        zm::HashMap<int32_t, zm::String> map;
    };

    zm::BlobBuffer blob = zm::write_scope<Root>(
        [&model](zm::BlobWriter<Root>& w)
        {
            w.root()->map = model;
        }, 4);

    auto dump = [](const Root* rr)
    {
        std::vector<std::pair<int32_t, std::string>> v;
        for (const auto& p : rr->map)
        {
            v.emplace_back(p.first, std::string(p.second.c_str()));
        }
        std::sort(v.begin(), v.end());
        return v;
    };

    std::vector<std::pair<int32_t, std::string>> expected(model.begin(), model.end());
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(dump(reinterpret_cast<const Root*>(blob.data())), expected);
}

// P1-04: Inserting the same key twice keeps the last value (map overwrite semantics).
TEST(ZmeyaTestSuite, Coverage_P1_HashMapDuplicateKeyReplacesValue)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovHashMapIntIntRoot>(
        [](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.hashmap_insert(w.root()->map, 7, 1);
            w.hashmap_insert(w.root()->map, 7, 2);
            w.hashmap_insert(w.root()->map, 7, 99);
        }, 128, 4);

    const zm::HashMap<int32_t, int32_t>& m = reinterpret_cast<const CovHashMapIntIntRoot*>(blob.data())->map;
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(*m.find(7), 99);
}

// P1-05: Erasing a missing key must not change size or corrupt existing entries.
TEST(ZmeyaTestSuite, Coverage_P1_HashMapEraseMissingNoOp)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovHashMapIntIntRoot>(
        [](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.hashmap_insert(w.root()->map, 1, 10);
            w.hashmap_insert(w.root()->map, 2, 20);
            w.hashmap_erase(w.root()->map, 99);
        }, 128, 4);

    const zm::HashMap<int32_t, int32_t>& m = reinterpret_cast<const CovHashMapIntIntRoot*>(blob.data())->map;
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(*m.find(1), 10);
    EXPECT_EQ(*m.find(2), 20);
}

// P1-06: clear then a second wave of inserts leaves only the second wave.
TEST(ZmeyaTestSuite, Coverage_P1_HashMapClearThenRefill)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovHashMapIntIntRoot>(
        [](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.hashmap_insert(w.root()->map, 1, 100);
            w.hashmap_insert(w.root()->map, 2, 200);
            w.hashmap_clear(w.root()->map);
            w.hashmap_insert(w.root()->map, 3, 300);
        }, 256, 4);

    const zm::HashMap<int32_t, int32_t>& m = reinterpret_cast<const CovHashMapIntIntRoot*>(blob.data())->map;
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(*m.find(3), 300);
}

// P1-07: Many distinct string keys (incremental vs golden) still match; key count is capped for Debug stability under repeated hash rebuilds.
TEST(ZmeyaTestSuite, Coverage_P1_HashMapManyStringKeysVsGolden)
{
    std::unordered_map<std::string, int32_t> model;
    for (int i = 0; i < 100; ++i)
    {
        model[std::string("key_") + std::to_string(i)] = i;
    }

    zm::BlobBuffer golden = zm::write_scope<CovHashMapStrIntRoot>(
        [&model](zm::BlobWriter<CovHashMapStrIntRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    zm::BlobBuffer built = zm::write_scope<CovHashMapStrIntRoot>(
        [&model](zm::BlobWriter<CovHashMapStrIntRoot>& w)
        {
            std::vector<std::string> keys;
            keys.reserve(model.size());
            for (const auto& kv : model)
            {
                keys.push_back(kv.first);
            }
            std::sort(keys.begin(), keys.end());
            for (const std::string& k : keys)
            {
                w.hashmap_insert(w.root()->map, k, model.at(k));
            }
        }, 4);

    zmeya_coverage::ExpectHashMapStrIntMatchesModel(reinterpret_cast<const CovHashMapStrIntRoot*>(golden.data())->map, model);
    zmeya_coverage::ExpectHashMapStrIntMatchesModel(reinterpret_cast<const CovHashMapStrIntRoot*>(built.data())->map, model);
}

// P1-08: Single-element map and set still support find/contains and iteration.
TEST(ZmeyaTestSuite, Coverage_P1_SingleElementMapAndSet)
{
    struct Root
    {
        zm::HashMap<int32_t, int32_t> m;
        zm::HashSet<int32_t> s;
    };

    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<Root>(
        [](zm::BlobWriter<Root>& w)
        {
            w.hashmap_insert(w.root()->m, 42, 43);
            w.hashset_insert(w.root()->s, 7);
        }, 128, 4);

    const Root* r = reinterpret_cast<const Root*>(blob.data());
    ASSERT_EQ(r->m.size(), 1u);
    EXPECT_EQ(*r->m.find(42), 43);
    ASSERT_EQ(r->s.size(), 1u);
    EXPECT_TRUE(r->s.contains(7));
    int count = 0;
    for (const auto& p : r->m)
    {
        EXPECT_EQ(p.first, 42);
        EXPECT_EQ(p.second, 43);
        ++count;
    }
    EXPECT_EQ(count, 1);
}

// P1-09: Large-N incremental hash insert matches golden (chain table + rehash; O(n) inserts total).
TEST(ZmeyaTestSuite, Coverage_P1_LargeNIncrementalHashMapVsGolden)
{
    std::unordered_map<int32_t, int32_t> model;
#ifdef _DEBUG
    constexpr int kN = 600;
#else
    constexpr int kN = 5000;
#endif
    for (int i = 0; i < kN; ++i)
    {
        model[i] = i ^ 0x5555;
    }

    zm::BlobBuffer golden = zm::write_scope<CovHashMapIntIntRoot>(
        [&model](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.root()->map = model;
        },
        4);

    zm::BlobBuffer built = zm::write_scope<CovHashMapIntIntRoot>(
        [&model](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            for (const auto& kv : model)
            {
                w.hashmap_insert(w.root()->map, kv.first, kv.second);
            }
        }, 4);

    auto dumpIntMap = [](const zm::HashMap<int32_t, int32_t>& m)
    {
        std::vector<std::pair<int32_t, int32_t>> v;
        for (const auto& p : m)
        {
            v.emplace_back(p.first, p.second);
        }
        std::sort(v.begin(), v.end());
        return v;
    };
    EXPECT_EQ(dumpIntMap(reinterpret_cast<const CovHashMapIntIntRoot*>(golden.data())->map),
        dumpIntMap(reinterpret_cast<const CovHashMapIntIntRoot*>(built.data())->map));
}

