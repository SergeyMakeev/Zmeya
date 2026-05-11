#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct IncrementalIntArrayRoot
{
    zm::Array<int32_t> values;
};

TEST(ZmeyaTestSuite, IncrementalArray_PushBackMatchesBulkAssign)
{
    std::vector<int32_t> src;
    for (int i = 0; i < 50; ++i)
    {
        src.push_back(i * 3 - 7);
    }

    std::vector<char> golden = zm::write_blob<IncrementalIntArrayRoot>(
        [&src](zm::BlobWriter<IncrementalIntArrayRoot>& w)
        {
            w.root()->values = src;
        },
        65536,
        4);

    std::vector<char> built = zm::write_blob<IncrementalIntArrayRoot>(
        [&src](zm::BlobWriter<IncrementalIntArrayRoot>& w)
        {
            for (int v : src)
            {
                w.array_push_back(w.root()->values, v);
            }
        },
        32,
        4);

    const IncrementalIntArrayRoot* rg = reinterpret_cast<const IncrementalIntArrayRoot*>(golden.data());
    const IncrementalIntArrayRoot* rb = reinterpret_cast<const IncrementalIntArrayRoot*>(built.data());
    ASSERT_EQ(rg->values.size(), rb->values.size());
    for (size_t i = 0; i < rg->values.size(); ++i)
    {
        EXPECT_EQ(rg->values[i], rb->values[i]);
    }
}

TEST(ZmeyaTestSuite, IncrementalArray_EraseResizePopBack)
{
    std::vector<char> blob = zm::write_blob<IncrementalIntArrayRoot>(
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
        },
        64,
        4);

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

TEST(ZmeyaTestSuite, IncrementalString_AppendMatchesAssign)
{
    std::vector<char> golden = zm::write_blob<IncrementalStringRoot>(
        [](zm::BlobWriter<IncrementalStringRoot>& w)
        {
            w.root()->text = std::string("hello world from zm");
        },
        4096,
        4);

    std::vector<char> built = zm::write_blob<IncrementalStringRoot>(
        [](zm::BlobWriter<IncrementalStringRoot>& w)
        {
            w.root()->text = std::string("hello ");
            w.string_append(w.root()->text, "world ");
            w.root()->text += "from zm";
        },
        32,
        4);

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

TEST(ZmeyaTestSuite, IncrementalHashMap_RebuildPathMatchesBulkAssign)
{
    std::unordered_map<std::string, int32_t> model = {{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};

    std::vector<char> golden = zm::write_blob<IncrementalHashMapRoot>(
        [&model](zm::BlobWriter<IncrementalHashMapRoot>& w)
        {
            w.root()->map = model;
        },
        65536,
        4);

    std::vector<char> built = zm::write_blob<IncrementalHashMapRoot>(
        [&model](zm::BlobWriter<IncrementalHashMapRoot>& w)
        {
            for (const auto& kv : model)
            {
                w.hashmap_insert(w.root()->map, kv.first, kv.second);
            }
        },
        128,
        4);

    auto g = ReadLogicalMap(reinterpret_cast<const IncrementalHashMapRoot*>(golden.data())->map);
    auto b = ReadLogicalMap(reinterpret_cast<const IncrementalHashMapRoot*>(built.data())->map);
    EXPECT_EQ(g, b);
}

TEST(ZmeyaTestSuite, IncrementalHashMap_EraseInsertOverwrite)
{
    std::vector<char> blob = zm::write_blob<IncrementalHashMapRoot>(
        [](zm::BlobWriter<IncrementalHashMapRoot>& w)
        {
            w.hashmap_insert(w.root()->map, std::string("x"), 1);
            w.hashmap_insert(w.root()->map, std::string("y"), 2);
            w.hashmap_erase(w.root()->map, std::string("x"));
            w.hashmap_insert(w.root()->map, std::string("y"), 99);
            w.hashmap_insert(w.root()->map, std::string("z"), 3);
        },
        256,
        4);

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

TEST(ZmeyaTestSuite, IncrementalHashSet_RebuildMatchesBulkAssign)
{
    std::unordered_set<int32_t> model = {3, 1, 4, 1, 5, 9, 2, 6};

    std::vector<char> golden = zm::write_blob<IncrementalHashSetRoot>(
        [&model](zm::BlobWriter<IncrementalHashSetRoot>& w)
        {
            w.root()->set = model;
        },
        65536,
        4);

    std::vector<char> built = zm::write_blob<IncrementalHashSetRoot>(
        [&model](zm::BlobWriter<IncrementalHashSetRoot>& w)
        {
            for (int v : model)
            {
                w.hashset_insert(w.root()->set, v);
            }
        },
        128,
        4);

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

TEST(ZmeyaTestSuite, IncrementalHashSet_EraseClear)
{
    std::vector<char> blob = zm::write_blob<IncrementalHashSetRoot>(
        [](zm::BlobWriter<IncrementalHashSetRoot>& w)
        {
            w.hashset_insert(w.root()->set, 10);
            w.hashset_insert(w.root()->set, 20);
            w.hashset_erase(w.root()->set, 10);
            w.hashset_insert(w.root()->set, 30);
            w.hashset_clear(w.root()->set);
            w.hashset_insert(w.root()->set, 7);
        },
        256,
        4);

    const IncrementalHashSetRoot* r = reinterpret_cast<const IncrementalHashSetRoot*>(blob.data());
    EXPECT_EQ(r->set.size(), 1u);
    EXPECT_TRUE(r->set.contains(7));
}
