#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct IncrementalIntArrayRoot
{
    zm::Array<int32_t> values;
};

// Verifies incremental array_push_back matches one-shot assign from the same STL vector (logical equality).
TEST(ZmeyaTestSuite, IncrementalArray_PushBackMatchesBulkAssign)
{
    std::vector<int32_t> src;
    for (int i = 0; i < 50; ++i)
    {
        src.push_back(i * 3 - 7);
    }

    zm::BlobBuffer golden = zm::write_scope<IncrementalIntArrayRoot>(
        [&src](zm::BlobWriter<IncrementalIntArrayRoot>& w)
        {
            w.root()->values = src;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<IncrementalIntArrayRoot>(
        [&src](zm::BlobWriter<IncrementalIntArrayRoot>& w)
        {
            for (int v : src)
            {
                w.array_push_back(w.root()->values, v);
            }
        }, 32, 4);

    const IncrementalIntArrayRoot* rg = reinterpret_cast<const IncrementalIntArrayRoot*>(golden.data());
    const IncrementalIntArrayRoot* rb = reinterpret_cast<const IncrementalIntArrayRoot*>(built.data());
    ASSERT_EQ(rg->values.size(), rb->values.size());
    for (size_t i = 0; i < rg->values.size(); ++i)
    {
        EXPECT_EQ(rg->values[i], rb->values[i]);
    }
}

// Exercises erase_at, pop_back, resize-with-fill, clear, then repopulate; checks final array contents match the intended sequence.
TEST(ZmeyaTestSuite, IncrementalArray_EraseResizePopBack)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<IncrementalIntArrayRoot>(
        [](zm::BlobWriter<IncrementalIntArrayRoot>& w)
        {
            for (int i = 0; i < 10; ++i)
            {
                w.array_push_back(w.root()->values, i);
            }
            w.array_erase_at(w.root()->values, 2);
            w.array_pop_back(w.root()->values);
            w.array_resize(w.root()->values, 20, int32_t(-1));
            w.array_clear(w.root()->values);
            for (int j = 0; j < 3; ++j)
            {
                w.array_push_back(w.root()->values, j * 10);
            }
        }, 64, 4);

    const IncrementalIntArrayRoot* r = reinterpret_cast<const IncrementalIntArrayRoot*>(blob.data());
    ASSERT_EQ(r->values.size(), 3u);
    EXPECT_EQ(r->values[0], 0);
    EXPECT_EQ(r->values[1], 10);
    EXPECT_EQ(r->values[2], 20);
}

struct IncrementalStringRoot
{
    zm::String text;
};

// Verifies string_append / operator+= under a tiny reserve produce the same C string as a single bulk assign (append path vs golden).
TEST(ZmeyaTestSuite, IncrementalString_AppendMatchesAssign)
{
    zm::BlobBuffer golden = zm::write_scope<IncrementalStringRoot>(
        [](zm::BlobWriter<IncrementalStringRoot>& w)
        {
            w.root()->text = std::string("hello world from zm");
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<IncrementalStringRoot>(
        [](zm::BlobWriter<IncrementalStringRoot>& w)
        {
            w.root()->text = std::string("hello ");
            w.string_append(w.root()->text, "world ");
            (void)(w.root()->text += "from zm");
        }, 32, 4);

    const IncrementalStringRoot* rg = reinterpret_cast<const IncrementalStringRoot*>(golden.data());
    const IncrementalStringRoot* rb = reinterpret_cast<const IncrementalStringRoot*>(built.data());
    EXPECT_STREQ(rg->text.c_str(), rb->text.c_str());
}

struct IncrementalHashMapRoot
{
    zm::HashMap<zm::String, int32_t> map;
};

static std::unordered_map<std::string, int32_t> ReadLogicalMap(const zm::HashMap<zm::String, int32_t>& m)
{
    std::unordered_map<std::string, int32_t> out;
    for (const auto& it : m)
    {
        out[std::string(it.first.c_str())] = it.second;
    }
    return out;
}

// Verifies repeated hashmap_insert with a small initial buffer matches bulk assign of the same logical map (string keys).
TEST(ZmeyaTestSuite, IncrementalHashMap_RebuildPathMatchesBulkAssign)
{
    std::unordered_map<std::string, int32_t> model = {{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};

    zm::BlobBuffer golden = zm::write_scope<IncrementalHashMapRoot>(
        [&model](zm::BlobWriter<IncrementalHashMapRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<IncrementalHashMapRoot>(
        [&model](zm::BlobWriter<IncrementalHashMapRoot>& w)
        {
            for (const auto& kv : model)
            {
                w.hashmap_insert(w.root()->map, kv.first, kv.second);
            }
        }, 128, 4);

    auto g = ReadLogicalMap(reinterpret_cast<const IncrementalHashMapRoot*>(golden.data())->map);
    auto b = ReadLogicalMap(reinterpret_cast<const IncrementalHashMapRoot*>(built.data())->map);
    EXPECT_EQ(g, b);
}

// Verifies erase, overwrite of an existing key, and insert of a new key leave the expected final map contents.
TEST(ZmeyaTestSuite, IncrementalHashMap_EraseInsertOverwrite)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<IncrementalHashMapRoot>(
        [](zm::BlobWriter<IncrementalHashMapRoot>& w)
        {
            w.hashmap_insert(w.root()->map, std::string("x"), 1);
            w.hashmap_insert(w.root()->map, std::string("y"), 2);
            w.hashmap_erase(w.root()->map, std::string("x"));
            w.hashmap_insert(w.root()->map, std::string("y"), 99);
            w.hashmap_insert(w.root()->map, std::string("z"), 3);
        }, 256, 4);

    const IncrementalHashMapRoot* r = reinterpret_cast<const IncrementalHashMapRoot*>(blob.data());
    auto m = ReadLogicalMap(r->map);
    ASSERT_EQ(m.size(), 2u);
    EXPECT_EQ(m["y"], 99);
    EXPECT_EQ(m["z"], 3);
}

struct IncrementalHashSetRoot
{
    zm::HashSet<int32_t> set;
};

// Verifies incremental hashset_insert for int keys matches bulk assign when canonicalized by sorting element lists.
TEST(ZmeyaTestSuite, IncrementalHashSet_RebuildMatchesBulkAssign)
{
    std::unordered_set<int32_t> model = {3, 1, 4, 1, 5, 9, 2, 6};

    zm::BlobBuffer golden = zm::write_scope<IncrementalHashSetRoot>(
        [&model](zm::BlobWriter<IncrementalHashSetRoot>& w)
        {
            w.root()->set = model;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<IncrementalHashSetRoot>(
        [&model](zm::BlobWriter<IncrementalHashSetRoot>& w)
        {
            for (int v : model)
            {
                w.hashset_insert(w.root()->set, v);
            }
        }, 128, 4);

    std::vector<int32_t> gvec;
    for (const int32_t& v : reinterpret_cast<const IncrementalHashSetRoot*>(golden.data())->set)
    {
        gvec.push_back(v);
    }
    std::vector<int32_t> bvec;
    for (const int32_t& v : reinterpret_cast<const IncrementalHashSetRoot*>(built.data())->set)
    {
        bvec.push_back(v);
    }
    std::sort(gvec.begin(), gvec.end());
    std::sort(bvec.begin(), bvec.end());
    EXPECT_EQ(gvec, bvec);
}

// Verifies erase, clear, then insert leaves exactly the post-clear wave of data in the set.
TEST(ZmeyaTestSuite, IncrementalHashSet_EraseClear)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<IncrementalHashSetRoot>(
        [](zm::BlobWriter<IncrementalHashSetRoot>& w)
        {
            w.hashset_insert(w.root()->set, 10);
            w.hashset_insert(w.root()->set, 20);
            w.hashset_erase(w.root()->set, 10);
            w.hashset_insert(w.root()->set, 30);
            w.hashset_clear(w.root()->set);
            w.hashset_insert(w.root()->set, 7);
        }, 256, 4);

    const IncrementalHashSetRoot* r = reinterpret_cast<const IncrementalHashSetRoot*>(blob.data());
    EXPECT_EQ(r->set.size(), 1u);
    EXPECT_TRUE(r->set.contains(7));
}

// Verifies hashset_reserve_nodes then inserts match bulk assign (same logical set).
TEST(ZmeyaTestSuite, IncrementalHashSet_ReserveNodesMatchesBulkAssign)
{
    std::unordered_set<int32_t> model = {3, 1, 4, 1, 5, 9, 2, 6};

    zm::BlobBuffer golden = zm::write_scope<IncrementalHashSetRoot>(
        [&model](zm::BlobWriter<IncrementalHashSetRoot>& w)
        {
            w.root()->set = model;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<IncrementalHashSetRoot>(
        [&model](zm::BlobWriter<IncrementalHashSetRoot>& w)
        {
            w.hashset_reserve_nodes(w.root()->set, model.size());
            for (int v : model)
            {
                w.hashset_insert(w.root()->set, v);
            }
        }, 128, 4);

    std::vector<int32_t> gvec;
    for (const int32_t& v : reinterpret_cast<const IncrementalHashSetRoot*>(golden.data())->set)
    {
        gvec.push_back(v);
    }
    std::vector<int32_t> bvec;
    for (const int32_t& v : reinterpret_cast<const IncrementalHashSetRoot*>(built.data())->set)
    {
        bvec.push_back(v);
    }
    std::sort(gvec.begin(), gvec.end());
    std::sort(bvec.begin(), bvec.end());
    EXPECT_EQ(gvec, bvec);
}

// Verifies hashmap_reserve_nodes (empty table hint) plus inserts match bulk assign for string keys.
TEST(ZmeyaTestSuite, IncrementalHashMap_ReserveNodesMatchesBulkAssign)
{
    std::unordered_map<std::string, int32_t> model = {{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};

    zm::BlobBuffer golden = zm::write_scope<IncrementalHashMapRoot>(
        [&model](zm::BlobWriter<IncrementalHashMapRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    zm::BlobBuffer built = zmeya_test::write_scope_stressed<IncrementalHashMapRoot>(
        [&model](zm::BlobWriter<IncrementalHashMapRoot>& w)
        {
            w.hashmap_reserve_nodes(w.root()->map, model.size());
            for (const auto& kv : model)
            {
                w.hashmap_insert(w.root()->map, kv.first, kv.second);
            }
        }, 128, 4);

    auto g = ReadLogicalMap(reinterpret_cast<const IncrementalHashMapRoot*>(golden.data())->map);
    auto b = ReadLogicalMap(reinterpret_cast<const IncrementalHashMapRoot*>(built.data())->map);
    EXPECT_EQ(g, b);
}

// Stresses many string replacements so dead ranges accumulate; finalize should compact and leave the final string correct with an empty dead-range list.
TEST(ZmeyaTestSuite, Compaction_FinalizeShrinksWorkingBufferAfterStringReplacements)
{
    auto builder = zm::detail::Builder<IncrementalStringRoot>::create();
    zm::detail::ScopedBuilder scope(builder.get());
    zm::ArenaRef<IncrementalStringRoot> root = builder->getRoot();
    size_t peak = 0;
    const int kRounds = 120;
    for (int i = 0; i < kRounds; ++i)
    {
        zm::assign(root->text, std::string(300, static_cast<char>('A' + (i % 26))));
        peak = (std::max)(peak, builder->data.size());
    }
    const std::string expected(300, static_cast<char>('A' + ((kRounds - 1) % 26)));
    const size_t pre_finalize = builder->data.size();
    ASSERT_FALSE(builder->dead_ranges_.empty());
    zm::Span<char> span = builder->finalize(4);
    const size_t post = span.size;
    EXPECT_LT(post, pre_finalize) << "seal-time compaction should drop dead string blobs";
    EXPECT_LT(post, peak) << "bump peak should exceed sealed size when compaction runs";
    EXPECT_TRUE(builder->dead_ranges_.empty());
    const IncrementalStringRoot* rr = reinterpret_cast<const IncrementalStringRoot*>(span.data);
    EXPECT_EQ(expected, std::string(rr->text.c_str()));
}
