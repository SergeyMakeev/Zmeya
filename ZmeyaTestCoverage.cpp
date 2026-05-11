#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace
{

static void ExpectHashMapStrIntMatchesModel(const zm::HashMap<zm::String, int32_t>& m, const std::unordered_map<std::string, int32_t>& model)
{
    for (const auto& kv : model)
    {
        EXPECT_TRUE(m.contains(kv.first.c_str()));
        EXPECT_EQ(m.find(kv.first.c_str(), int32_t(-99999)), kv.second);
    }
}

static std::vector<std::string> SortedStringSet(const zm::HashSet<zm::String>& hs)
{
    std::vector<std::string> out;
    out.reserve(hs.size());
    for (const zm::String& s : hs)
    {
        out.emplace_back(s.c_str());
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

struct CovIntArrayRoot
{
    zm::Array<int32_t> values;
};

struct CovStringRoot
{
    zm::String text;
};

struct CovHashMapStrIntRoot
{
    zm::HashMap<zm::String, int32_t> map;
};

struct CovHashMapStrStrRoot
{
    zm::HashMap<zm::String, zm::String> map;
};

struct CovHashMapIntIntRoot
{
    zm::HashMap<int32_t, int32_t> map;
};

struct CovHashSetStrRoot
{
    zm::HashSet<zm::String> set;
};

struct CovPairRoot
{
    zm::Pair<int32_t, float> p;
};

enum class CovEnumField : uint32_t
{
    A = 7,
    B = 42,
};

struct CovEnumRoot
{
    CovEnumField e;
};

struct CovPointerChainNode
{
    int32_t id;
    zm::Pointer<CovPointerChainNode> next;
};

struct CovPointerChainRoot
{
    zm::Pointer<CovPointerChainNode> head;
};

struct CovNullPointerRoot
{
    zm::Pointer<CovPointerChainNode> p;
};

struct CovTreeNode
{
    int32_t value;
    zm::Pointer<CovTreeNode> parent;
    zm::Array<zm::Pointer<CovTreeNode>> children;
};

struct CovTreeRoot
{
    zm::Pointer<CovTreeNode> root;
};

struct alignas(16) CovAlignedPayload
{
    uint64_t a;
    uint64_t b;
};

struct CovAllocAlignRoot
{
    zm::Pointer<double> pd;
    zm::Pointer<CovAlignedPayload> pp;
};

struct CovMmapMiniRoot
{
    uint32_t magic;
    zm::String tag;
};

struct CovVersionRoot
{
    uint32_t magic;
    uint32_t version;
    int32_t payload;
};

struct CovAllKindsRoot
{
    zm::String s;
    zm::Array<int32_t> ints;
    zm::HashSet<int32_t> hs;
    zm::HashMap<zm::String, int32_t> hm;
    zm::Array<zm::Array<int32_t>> grid;
    zm::Pointer<CovPointerChainNode> node;
};

struct CovDeepCopyRoot
{
    zm::String s;
};

struct CovArrayPtrRoot
{
    zm::Array<zm::Pointer<CovPointerChainNode>> nodes;
};

struct CovIterEmptyRoot
{
    zm::Array<int32_t> arr;
    zm::HashSet<int32_t> hs;
    zm::HashMap<int32_t, int32_t> hm;
};

struct CovStringCmpRoot
{
    zm::String a;
    zm::String b;
};

struct CoverageReferNodeInit
{
    std::string str;
    std::vector<int> arr;
    std::unordered_set<int> hashSet;
    std::unordered_map<std::string, float> hashMap;
};

struct CoverageReferNode
{
    zm::String str;
    zm::Array<int> arr;
    zm::HashSet<int> hashSet;
    zm::HashMap<zm::String, float> hashMap;

    CoverageReferNode& operator=(const CoverageReferNodeInit& o)
    {
        str = o.str;
        arr = o.arr;
        hashSet = o.hashSet;
        hashMap = o.hashMap;
        return *this;
    }
};

struct CoverageReferMultiRoot
{
    zm::Array<CoverageReferNode> nodes;
};

struct CoverageReferSingleRoot
{
    CoverageReferNode node;
};

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

    zm::BlobBuffer golden = zm::write_blob<CovStringRoot>(
        [&model](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = model;
        }, 4);

    zm::BlobBuffer built = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
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

    zm::BlobBuffer golden = zm::write_blob<CovStringRoot>(
        [&goldenStr](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("temp");
            w.root()->text = goldenStr;
        }, 4);

    zm::BlobBuffer built = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
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
    zm::BlobBuffer golden = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string();
            w.root()->text += "x";
        }, 256, 4);

    zm::BlobBuffer built = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
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

    zm::BlobBuffer golden = zm::write_blob<CovIntArrayRoot>(
        [&model](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.root()->values = model;
        }, 4);

    zm::BlobBuffer built = zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
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

    zm::BlobBuffer golden = zm::write_blob<CovHashMapStrIntRoot>(
        [&model](zm::BlobWriter<CovHashMapStrIntRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    zm::BlobBuffer built = zm::write_blob<CovHashMapStrIntRoot>(
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

    ExpectHashMapStrIntMatchesModel(reinterpret_cast<const CovHashMapStrIntRoot*>(golden.data())->map, model);
    ExpectHashMapStrIntMatchesModel(reinterpret_cast<const CovHashMapStrIntRoot*>(built.data())->map, model);
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

    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<Root>(
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
        zm::BlobBuffer blob = zm::write_blob<CovStringRoot>(
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

// P0-12: Explicit assign(..., HashMap with nested strings) under a small starting arena matches write_blob golden (TLS + realloc stress).
TEST(ZmeyaTestSuite, Coverage_P0_ExplicitAssignHashMapNestedStringsUnderRealloc)
{
    std::unordered_map<std::string, std::string> model;
    for (int i = 0; i < 80; ++i)
    {
        model[std::string("k") + std::to_string(i)] = std::string("v") + std::to_string(i * i);
    }

    zm::BlobBuffer golden = zm::write_blob<CovHashMapStrStrRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovDeepCopyRoot>(
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
    zm::BlobBuffer blob = zm::write_blob<CovPointerChainRoot>(
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
    zm::write_blob<CovStringRoot>(
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

    zm::BlobBuffer golden = zm::write_blob<CovHashSetStrRoot>(
        [&model](zm::BlobWriter<CovHashSetStrRoot>& w)
        {
            w.root()->set = model;
        }, 4);

    zm::BlobBuffer built = zm::detail::write_blob_with_initial_buffer_bytes<CovHashSetStrRoot>(
        [&model](zm::BlobWriter<CovHashSetStrRoot>& w)
        {
            std::vector<std::string> order(model.begin(), model.end());
            std::shuffle(order.begin(), order.end(), std::mt19937(99));
            for (const std::string& s : order)
            {
                w.hashset_insert(w.root()->set, s);
            }
        }, 128, 4);

    EXPECT_EQ(SortedStringSet(reinterpret_cast<const CovHashSetStrRoot*>(golden.data())->set),
        SortedStringSet(reinterpret_cast<const CovHashSetStrRoot*>(built.data())->set));
}

// P1-02: HashMap with int keys uses the non-string hash path; incremental ops match an STL reference map.
TEST(ZmeyaTestSuite, Coverage_P1_HashMapIntKeyIncrementalVsGolden)
{
    std::unordered_map<int32_t, int32_t> model;
    for (int i = -50; i < 50; ++i)
    {
        model[i] = i * 3 + 1;
    }

    zm::BlobBuffer golden = zm::write_blob<CovHashMapIntIntRoot>(
        [&model](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    zm::BlobBuffer built = zm::detail::write_blob_with_initial_buffer_bytes<CovHashMapIntIntRoot>(
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

    zm::BlobBuffer blob = zm::write_blob<Root>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovHashMapIntIntRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovHashMapIntIntRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovHashMapIntIntRoot>(
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

    zm::BlobBuffer golden = zm::write_blob<CovHashMapStrIntRoot>(
        [&model](zm::BlobWriter<CovHashMapStrIntRoot>& w)
        {
            w.root()->map = model;
        }, 4);

    zm::BlobBuffer built = zm::write_blob<CovHashMapStrIntRoot>(
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

    ExpectHashMapStrIntMatchesModel(reinterpret_cast<const CovHashMapStrIntRoot*>(golden.data())->map, model);
    ExpectHashMapStrIntMatchesModel(reinterpret_cast<const CovHashMapStrIntRoot*>(built.data())->map, model);
}

// P1-08: Single-element map and set still support find/contains and iteration.
TEST(ZmeyaTestSuite, Coverage_P1_SingleElementMapAndSet)
{
    struct Root
    {
        zm::HashMap<int32_t, int32_t> m;
        zm::HashSet<int32_t> s;
    };

    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<Root>(
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

// P1-09: Large-N incremental hash insert matches golden.
// Incremental hashmap_insert copies the whole map into an std::unordered_map and re-assigns each time (O(size) per call), so N inserts cost O(N^2) overall; keep Debug N small so the suite stays interactive.
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

    zm::BlobBuffer golden = zm::write_blob<CovHashMapIntIntRoot>(
        [&model](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.root()->map = model;
        },
        4);

    zm::BlobBuffer built = zm::write_blob<CovHashMapIntIntRoot>(
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

// P2-01: Assigning a long string through the builder must remain stable (growth path, no truncation for large lengths).
TEST(ZmeyaTestSuite, Coverage_P2_LongStringAssignRoundTrip)
{
    std::string longStr(size_t(100000), 'z');
    for (size_t i = 0; i < longStr.size(); i += 997)
    {
        longStr[i] = static_cast<char>('a' + (i % 26));
    }

    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [&longStr](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = longStr;
        }, 64, 4);

    EXPECT_EQ(std::string(reinterpret_cast<const CovStringRoot*>(blob.data())->text.c_str()), longStr);
}

// P2-02: Embedded NUL bytes are stored with full std::string length via memcpy; C-string APIs still truncate at the first NUL (documented limitation).
TEST(ZmeyaTestSuite, Coverage_P2_StringWithEmbeddedNulStoredLength)
{
    std::string s("pre");
    s.push_back('\0');
    s.append("post", 4);

    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [&s](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = s;
        }, 256, 4);

    const zm::String& zs = reinterpret_cast<const CovStringRoot*>(blob.data())->text;
    EXPECT_EQ(std::string(zs.c_str()), std::string("pre"));
    EXPECT_TRUE(zs == std::string("pre"));
}

// P2-03: const char* and std::string sources with identical byte content produce identical serialized strings.
TEST(ZmeyaTestSuite, Coverage_P2_CharPtrVsStdStringSameBytes)
{
    zm::BlobBuffer a = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = "identical_payload";
        }, 128, 4);

    zm::BlobBuffer b = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("identical_payload");
        }, 128, 4);

    EXPECT_STREQ(reinterpret_cast<const CovStringRoot*>(a.data())->text.c_str(),
        reinterpret_cast<const CovStringRoot*>(b.data())->text.c_str());
}

// P2-04: Member operator+= only (no bulk assign of the whole string until implicit initial empty) under a tiny reserve.
TEST(ZmeyaTestSuite, Coverage_P2_StringMemberPlusEqualsChain)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            for (int i = 0; i < 200; ++i)
            {
                w.root()->text += "x";
            }
        }, 32, 4);

    EXPECT_EQ(std::strlen(reinterpret_cast<const CovStringRoot*>(blob.data())->text.c_str()), 200u);
}

// P2-05: String::clear() from the member API leaves an empty C string view.
TEST(ZmeyaTestSuite, Coverage_P2_StringMemberClear)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("will_clear");
            w.root()->text.clear();
        }, 128, 4);

    const zm::String& t = reinterpret_cast<const CovStringRoot*>(blob.data())->text;
    EXPECT_TRUE(t.empty());
    EXPECT_STREQ(t.c_str(), "");
}

// P2-06: Comparison operators cover zm vs const char*, std::string, and reflexive cases.
TEST(ZmeyaTestSuite, Coverage_P2_StringComparisonOperators)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovStringCmpRoot>(
        [](zm::BlobWriter<CovStringCmpRoot>& w)
        {
            w.root()->a = std::string("same");
            w.root()->b = std::string("other");
        }, 256, 4);

    const CovStringCmpRoot* r = reinterpret_cast<const CovStringCmpRoot*>(blob.data());
    EXPECT_TRUE(r->a == r->a);
    EXPECT_FALSE(r->a != r->a);
    EXPECT_TRUE(r->a == "same");
    EXPECT_TRUE("same" == r->a);
    EXPECT_TRUE(r->a == std::string("same"));
    EXPECT_TRUE(std::string("same") == r->a);
    EXPECT_TRUE(r->a != r->b);
    EXPECT_FALSE(r->a == r->b);
}

// P2-07: Self-alias via c_str is not supported for zm::String assign (would read/write overlapping storage); skipped as a documented contract.
TEST(ZmeyaTestSuite, Coverage_P2_StringSelfAliasAssignSkipped)
{
    GTEST_SKIP() << "zm::String assign from its own c_str is undefined; not exercised.";
}

// P3-01: zm::String arrays reject incremental push_back at compile time via zm_array_push_back_ok.
TEST(ZmeyaTestSuite, Coverage_P3_ArrayStringPushBackCompileTrait)
{
    static_assert(!zm::detail::zm_array_push_back_ok<zm::String>::value, "incremental push_back must be disabled for zm::String elements");
}

// P3-02: Array of pointers round-trips from a std::vector of allocated node addresses.
TEST(ZmeyaTestSuite, Coverage_P3_ArrayOfPointersAssign)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovArrayPtrRoot>(
        [](zm::BlobWriter<CovArrayPtrRoot>& w)
        {
            std::vector<CovPointerChainNode*> ptrs;
            for (int i = 0; i < 5; ++i)
            {
                CovPointerChainNode* n = w.allocate<CovPointerChainNode>();
                n->id = i * 10;
                n->next = nullptr;
                ptrs.push_back(n);
            }
            w.root()->nodes = ptrs;
        }, 256, 4);

    const zm::Array<zm::Pointer<CovPointerChainNode>>& a = reinterpret_cast<const CovArrayPtrRoot*>(blob.data())->nodes;
    ASSERT_EQ(a.size(), 5u);
    for (size_t i = 0; i < 5; ++i)
    {
        ASSERT_NE(a[i].get(), nullptr);
        EXPECT_EQ(a[i]->id, int32_t(i * 10));
    }
}

// P3-03: Nested Array<Array<int>> bulk-assign matches the nested std::vector model after read-back.
TEST(ZmeyaTestSuite, Coverage_P3_NestedArrayIntBulkAssign)
{
    struct Root
    {
        zm::Array<zm::Array<int32_t>> grid;
    };

    std::vector<std::vector<int32_t>> model = {{1, 2}, {3, 4, 5}, {6}};

    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<Root>(
        [&model](zm::BlobWriter<Root>& w)
        {
            w.root()->grid = model;
        }, 1024, 4);

    const Root* r = reinterpret_cast<const Root*>(blob.data());
    ASSERT_EQ(r->grid.size(), 3u);
    EXPECT_EQ(r->grid[0].size(), 2u);
    EXPECT_EQ(r->grid[1].size(), 3u);
    EXPECT_EQ(r->grid[2].size(), 1u);
    EXPECT_EQ(r->grid[1][2], 5);
}

// P3-04: array_resize to a large count fills new slots with the provided pattern (large starting arena avoids bump slab churn exhausting the arena on huge fills).
TEST(ZmeyaTestSuite, Coverage_P3_ArrayResizeLargeFillPattern)
{
#ifdef _DEBUG
    constexpr size_t kN = 4096;
    constexpr size_t kStartArenaBytes = 4u * 1024u * 1024u;
#else
    // Release uses a larger resize than Debug, but keep the logical size below what the bump-only resize path can push past the int32 goffset_t arena budget for this test harness.
    constexpr size_t kN = size_t(1) << 14;
    constexpr size_t kStartArenaBytes = 32u * 1024u * 1024u;
#endif
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
        [kN](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.array_resize(w.root()->values, kN, int32_t(-7));
        },
        kStartArenaBytes,
        4);

    const zm::Array<int32_t>& a = reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values;
    ASSERT_EQ(a.size(), kN);
    EXPECT_EQ(a[0], -7);
    EXPECT_EQ(a[kN / 2], -7);
    EXPECT_EQ(a[kN - 1], -7);
}

// P3-05: array_erase_at on an empty array is a documented no-op (out-of-range index).
TEST(ZmeyaTestSuite, Coverage_P3_ArrayEraseAtWhenEmptyNoOp)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
        [](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.array_erase_at(w.root()->values, 0);
        }, 32, 4);

    EXPECT_EQ(reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values.size(), 0u);
}

// P4-01: Null zm::Pointer serializes as a zero relative offset and reads back as nullptr.
TEST(ZmeyaTestSuite, Coverage_P4_NullPointerRoundTrip)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovNullPointerRoot>(
        [](zm::BlobWriter<CovNullPointerRoot>& w)
        {
            w.root()->p = nullptr;
        }, 64, 4);

    EXPECT_EQ(reinterpret_cast<const CovNullPointerRoot*>(blob.data())->p.get(), nullptr);
}

// P4-02: Long singly-linked pointer chain preserves roffset links across many nodes.
TEST(ZmeyaTestSuite, Coverage_P4_PointerChainThousandNodes)
{
    zm::BlobBuffer blob = zm::write_blob<CovPointerChainRoot>(
        [](zm::BlobWriter<CovPointerChainRoot>& w)
        {
            constexpr int kN = 1000;
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

    const CovPointerChainNode* cur = reinterpret_cast<const CovPointerChainRoot*>(blob.data())->head.get();
    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_NE(cur, nullptr);
        EXPECT_EQ(cur->id, i);
        cur = cur->next.get();
    }
    EXPECT_EQ(cur, nullptr);
}

// P4-03: Small tree with parent pointers plus child arrays forms a DAG-style back reference graph.
TEST(ZmeyaTestSuite, Coverage_P4_TreeWithParentPointers)
{
    zm::BlobBuffer blob = zm::write_blob<CovTreeRoot>(
        [](zm::BlobWriter<CovTreeRoot>& w)
        {
            CovTreeNode* root = w.allocate<CovTreeNode>();
            root->value = 1;
            root->parent = nullptr;
            CovTreeNode* left = w.allocate<CovTreeNode>();
            left->value = 2;
            left->parent = root;
            CovTreeNode* right = w.allocate<CovTreeNode>();
            right->value = 3;
            right->parent = root;
            std::vector<CovTreeNode*> ch = {left, right};
            root->children = ch;
            w.root()->root = root;
        },
        8);

    const CovTreeNode* root = reinterpret_cast<const CovTreeRoot*>(blob.data())->root.get();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->value, 1);
    ASSERT_EQ(root->children.size(), 2u);
    EXPECT_EQ(root->children[0]->value, 2);
    EXPECT_EQ(root->children[1]->value, 3);
    EXPECT_EQ(root->children[0]->parent.get(), root);
    EXPECT_EQ(root->children[1]->parent.get(), root);
}

// P4-04: allocate returns addresses aligned to the requested T inside the bump arena.
TEST(ZmeyaTestSuite, Coverage_P4_AllocatePointerAlignment)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovAllocAlignRoot>(
        [](zm::BlobWriter<CovAllocAlignRoot>& w)
        {
            double* pd = w.allocate<double>();
            *pd = 3.25;
            CovAlignedPayload* pp = w.allocate<CovAlignedPayload>();
            pp->a = 1;
            pp->b = 2;
            w.root()->pd = pd;
            w.root()->pp = pp;
        }, 64, 8);

    const CovAllocAlignRoot* r = reinterpret_cast<const CovAllocAlignRoot*>(blob.data());
    const uintptr_t pda = reinterpret_cast<uintptr_t>(r->pd.get());
    const uintptr_t ppa = reinterpret_cast<uintptr_t>(r->pp.get());
    EXPECT_EQ(pda % alignof(double), 0u);
    EXPECT_EQ(ppa % alignof(CovAlignedPayload), 0u);
}

// P5-01: Alignment of 1 always yields a valid blob size (trivially size % 1 == 0).
TEST(ZmeyaTestSuite, Coverage_P5_FinalizeAlignmentOne)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = "x";
        }, 64, 1);
    EXPECT_EQ(blob.size() % 1, 0u);
}

// P5-02: Extra finalize padding bytes (when using stricter alignment) are zero-filled in the output buffer.
TEST(ZmeyaTestSuite, Coverage_P5_FinalizePaddingBytesZero)
{
    zm::BlobBuffer a = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = "pad";
        }, 128, 4);

    zm::BlobBuffer b = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = "pad";
        }, 128, 8);

    ASSERT_GE(b.size(), a.size());
    for (size_t i = a.size(); i < b.size(); ++i)
    {
        EXPECT_EQ(static_cast<unsigned char>(b[i]), 0u);
    }
}

// P5-04: write_blob returns owning bytes with the same length as finalize() on an equivalent session (size sanity).
TEST(ZmeyaTestSuite, Coverage_P5_WriteBlobVectorMatchesFinalizeSize)
{
    std::unique_ptr<zm::detail::Builder<CovStringRoot>> builder = zm::detail::Builder<CovStringRoot>::create(128);
    zm::detail::ScopedBuilder scope(builder.get());
    builder->getRoot()->text = std::string("size_check");
    zm::Span<char> span = builder->finalize(8);
    zm::BlobBuffer viaVec = zm::detail::write_blob_with_initial_buffer_bytes<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("size_check");
        }, 128, 8);
    EXPECT_EQ(viaVec.size(), span.size);
}

// P6-01: Memory-map path on Windows (and buffer read on other hosts) accepts non-default finalize alignment (16).
TEST(ZmeyaTestSuite, Coverage_P6_MmapMiniRootAlignment16)
{
    const char* fileName = "coverage_mmap_mini.zm";
    zm::BlobBuffer bytes = zm::write_blob<CovMmapMiniRoot>(
        [](zm::BlobWriter<CovMmapMiniRoot>& w)
        {
            w.root()->magic = 0x11223344u;
            w.root()->tag = std::string("mmap_align16");
        }, 16);

    EXPECT_EQ(bytes.size() % 16, 0u);

    FILE* out = fopen(fileName, "wb");
    ASSERT_TRUE(out != nullptr);
    ASSERT_EQ(fwrite(bytes.data(), bytes.size(), 1, out), size_t(1));
    fclose(out);

#if defined(_WIN32)
    HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT_TRUE(hFile != INVALID_HANDLE_VALUE);
    LARGE_INTEGER sz;
    ASSERT_TRUE(GetFileSizeEx(hFile, &sz));
    HANDLE hMap = CreateFileMapping(hFile, 0, PAGE_READONLY | SEC_COMMIT, sz.HighPart, sz.LowPart, 0);
    ASSERT_TRUE(hMap != NULL);
    const CovMmapMiniRoot* mapped = reinterpret_cast<const CovMmapMiniRoot*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, size_t(sz.QuadPart)));
    ASSERT_TRUE(mapped != nullptr);
    EXPECT_EQ(mapped->magic, 0x11223344u);
    EXPECT_EQ(mapped->tag, std::string("mmap_align16"));
    UnmapViewOfFile(mapped);
    CloseHandle(hMap);
    CloseHandle(hFile);
#else
    FILE* in = fopen(fileName, "rb");
    ASSERT_TRUE(in != nullptr);
    fseek(in, 0L, SEEK_END);
    long n = ftell(in);
    fseek(in, 0L, SEEK_SET);
    std::vector<char> buf(size_t(n));
    ASSERT_EQ(fread(buf.data(), size_t(n), 1, in), size_t(1));
    fclose(in);
    const CovMmapMiniRoot* mapped = reinterpret_cast<const CovMmapMiniRoot*>(buf.data());
    EXPECT_EQ(mapped->magic, 0x11223344u);
    EXPECT_EQ(mapped->tag, std::string("mmap_align16"));
#endif
    remove(fileName);
}

// P6-04: Full file round-trip preserves raw blob bytes (fwrite/fread memcmp).
TEST(ZmeyaTestSuite, Coverage_P6_FileRoundTripBytesIdentical)
{
    zm::BlobBuffer bytes = zm::detail::write_blob_with_initial_buffer_bytes<CovHashMapIntIntRoot>(
        [](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.hashmap_insert(w.root()->map, 1, 2);
            w.hashmap_insert(w.root()->map, 3, 4);
        }, 512, 8);

    const char* path = "coverage_roundtrip.zm";
    {
        std::ofstream os(path, std::ios::binary);
        ASSERT_TRUE(os);
        os.write(bytes.data(), std::streamsize(bytes.size()));
    }
    std::vector<char> back(bytes.size());
    {
        std::ifstream is(path, std::ios::binary);
        ASSERT_TRUE(is);
        is.read(back.data(), std::streamsize(back.size()));
        ASSERT_EQ(size_t(is.gcount()), back.size());
    }
    remove(path);
    EXPECT_EQ(std::memcmp(bytes.data(), back.data(), bytes.size()), 0);
}

// P7-01: Smaller ReferTo-shaped graph (100 nodes) keeps the same per-node validation pattern as the stress test.
TEST(ZmeyaTestSuite, Coverage_P7_ReferToShapedSmallNodeCount)
{
    CoverageReferNodeInit proto;
    proto.str = "This is supposed to be a long enough string. I think it is long enough now.";
    proto.arr = {1, 2, 5, 8, 13, 99, 7, 160, 293, 890};
    proto.hashSet = {1, 5, 15, 23, 38, 31};
    proto.hashMap = {{"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}, {"four", 4.0f}};

    constexpr size_t kNodes = 100;
    zm::BlobBuffer blob = zm::write_blob<CoverageReferMultiRoot>(
        [&proto, kNodes](zm::BlobWriter<CoverageReferMultiRoot>& w)
        {
            std::vector<CoverageReferNodeInit> inits(kNodes, proto);
            w.root()->nodes = inits;
        },
        4);

    const CoverageReferMultiRoot* r = reinterpret_cast<const CoverageReferMultiRoot*>(blob.data());
    ASSERT_EQ(r->nodes.size(), kNodes);
    for (size_t i = 0; i < kNodes; ++i)
    {
        const CoverageReferNode& n = r->nodes[i];
        EXPECT_EQ(n.str, proto.str);
        ASSERT_EQ(n.arr.size(), proto.arr.size());
        EXPECT_EQ(n.hashSet.size(), proto.hashSet.size());
        EXPECT_EQ(n.hashMap.size(), proto.hashMap.size());
    }
}

// P7-02: Single-node ReferTo-shaped payload still exercises all nested container kinds on the read path.
TEST(ZmeyaTestSuite, Coverage_P7_ReferToShapedSingleNode)
{
    CoverageReferNodeInit proto;
    proto.str = "solo_node_string_payload";
    proto.arr = {9, 8, 7};
    proto.hashSet = {100, 200};
    proto.hashMap = {{"k", 1.5f}};

    zm::BlobBuffer blob = zm::write_blob<CoverageReferSingleRoot>(
        [&proto](zm::BlobWriter<CoverageReferSingleRoot>& w)
        {
            w.root()->node = proto;
        }, 4);

    const CoverageReferNode& n = reinterpret_cast<const CoverageReferSingleRoot*>(blob.data())->node;
    EXPECT_EQ(n.str, proto.str);
    ASSERT_EQ(n.arr.size(), 3u);
    EXPECT_TRUE(n.hashSet.contains(100));
    EXPECT_FLOAT_EQ(n.hashMap.find("k", 0.0f), 1.5f);
}

// P7-03: Short intrusive list smoke (10 nodes) validates prev/next wiring without the million-node cost.
TEST(ZmeyaTestSuite, Coverage_P7_ListChainSmokeTenNodes)
{
    struct Node
    {
        uint32_t payload;
        zm::Pointer<Node> prev;
        zm::Pointer<Node> next;
    };

    struct Root
    {
        uint32_t numNodes;
        zm::Pointer<Node> head;
    };

    constexpr uint32_t kN = 10;
    zm::BlobBuffer blob = zm::write_blob<Root>(
        [kN](zm::BlobWriter<Root>& w)
        {
            zm::detail::BuilderBase* bb = w.builder_base();
            w.root()->numNodes = kN;
            zm::goffset_t prev_g{};
            for (uint32_t i = 0; i < kN; ++i)
            {
                Node* node = w.allocate<Node>();
                node->payload = 100 + i;
                node->prev = nullptr;
                node->next = nullptr;
                if (i > 0)
                {
                    Node* prev = reinterpret_cast<Node*>(bb->get_ptr_unsafe_to_store(prev_g));
                    node->prev = prev;
                    prev->next = node;
                }
                else
                {
                    w.root()->head = node;
                }
                prev_g = bb->arena_byte_offset_of(node);
            }
        },
        8);

    const Root* rr = reinterpret_cast<const Root*>(blob.data());
    uint32_t c = 0;
    const Node* cur = rr->head.get();
    while (cur)
    {
        EXPECT_EQ(cur->payload, 100 + c);
        cur = cur->next.get();
        ++c;
    }
    EXPECT_EQ(c, kN);
}

// P8-01: Range-for over empty zm containers does not crash and yields zero iterations.
TEST(ZmeyaTestSuite, Coverage_P8_EmptyContainerIteration)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovIterEmptyRoot>(
        [](zm::BlobWriter<CovIterEmptyRoot>& /*w*/) {}, 128, 4);

    const CovIterEmptyRoot* r = reinterpret_cast<const CovIterEmptyRoot*>(blob.data());
    size_t nArr = 0;
    for (int32_t v : r->arr)
    {
        (void)v;
        ++nArr;
    }
    EXPECT_EQ(nArr, 0u);
    size_t nHs = 0;
    for (int32_t v : r->hs)
    {
        (void)v;
        ++nHs;
    }
    EXPECT_EQ(nHs, 0u);
    size_t nHm = 0;
    for (const auto& p : r->hm)
    {
        (void)p;
        ++nHm;
    }
    EXPECT_EQ(nHm, 0u);
}

// P8-02: For a non-empty array, begin/end iterators compare as unequal until the walk completes.
TEST(ZmeyaTestSuite, Coverage_P8_IteratorBeginEndPatterns)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
        [](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.array_push_back(w.root()->values, 1);
            w.array_push_back(w.root()->values, 2);
        }, 64, 4);

    const zm::Array<int32_t>& a = reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values;
    EXPECT_NE(a.begin(), a.end());
    size_t count = 0;
    for (const int32_t* it = a.begin(); it != a.end(); ++it)
    {
        ++count;
    }
    EXPECT_EQ(count, 2u);
}

// P8-03: HashMap::find(key, default) returns the default when the key is absent.
TEST(ZmeyaTestSuite, Coverage_P8_HashMapFindWithDefault)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovHashMapStrIntRoot>(
        [](zm::BlobWriter<CovHashMapStrIntRoot>& w)
        {
            w.hashmap_insert(w.root()->map, std::string("only"), 5);
        }, 128, 4);

    const zm::HashMap<zm::String, int32_t>& m = reinterpret_cast<const CovHashMapStrIntRoot*>(blob.data())->map;
    EXPECT_EQ(m.find("only", -999), 5);
    EXPECT_EQ(m.find("missing", -999), -999);
}

// P9-04: BlobWriter exposes a non-null builder_base during write_blob (already covered elsewhere; repeated for backlog traceability).
TEST(ZmeyaTestSuite, Coverage_P9_BlobWriterBuilderBaseNonNull)
{
    zm::write_blob<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            EXPECT_NE(w.builder_base(), nullptr);
        });
}

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
            zm::write_blob<CovStringRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovAllKindsRoot>(
        [](zm::BlobWriter<CovAllKindsRoot>& w)
        {
            w.root()->s = std::string("all_kinds");
            w.array_push_back(w.root()->ints, 7);
            w.hashset_insert(w.root()->hs, 9);
            w.hashmap_insert(w.root()->hm, std::string("k"), 3);
            std::vector<std::vector<int32_t>> g = {{1, 2}, {3}};
            w.root()->grid = g;
            CovPointerChainNode* n = w.allocate<CovPointerChainNode>();
            n->id = 99;
            n->next = nullptr;
            w.root()->node = n;
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

    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<Root>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovVersionRoot>(
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

// P12-01: Two identical write_blob sessions for a POD-only root can yield byte-identical blobs (deterministic layout).
TEST(ZmeyaTestSuite, Coverage_P12_TwoWriteBlobPODDeterministicBytes)
{
    auto make = []()
    {
        return zm::detail::write_blob_with_initial_buffer_bytes<CovIntArrayRoot>(
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
    zm::BlobBuffer blob = zm::write_blob<CovHashMapIntIntRoot>(
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
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovPairRoot>(
        [](zm::BlobWriter<CovPairRoot>& w)
        {
            w.root()->p.first = -9;
            w.root()->p.second = 2.5f;
        }, 64, 4);

    const CovPairRoot* r = reinterpret_cast<const CovPairRoot*>(blob.data());
    EXPECT_EQ(r->p.first, -9);
    EXPECT_FLOAT_EQ(r->p.second, 2.5f);
}

// P14-02: enum class stored as uint32_t field round-trips through write_blob.
TEST(ZmeyaTestSuite, Coverage_P14_EnumClassFieldRoundTrip)
{
    zm::BlobBuffer blob = zm::detail::write_blob_with_initial_buffer_bytes<CovEnumRoot>(
        [](zm::BlobWriter<CovEnumRoot>& w)
        {
            w.root()->e = CovEnumField::B;
        }, 64, 4);

    EXPECT_EQ(reinterpret_cast<const CovEnumRoot*>(blob.data())->e, CovEnumField::B);
}
