#pragma once

/*
**Coverage test shared types**

Root structs and helpers used by split ZmeyaTestCoverage_*.cpp translation units.

**Where tests live (P-number index)**

`ZmeyaTestCoverage_P0P1.cpp`: P0 string/array/hash incremental stress vs golden; P1 hash set/map behavior.
`ZmeyaTestCoverage_P2P4.cpp`: P2 string edge cases and comparisons; P3 arrays and nested arrays; P4 pointers and graphs.
`ZmeyaTestCoverage_P5P9.cpp`: P5 finalize alignment and write_scope sizing; P6 mmap/file IO; P7 smaller graph reads; P8 iterators and find; P9 BlobWriter builder_base smoke.
`ZmeyaTestCoverage_DeathAndTail.cpp`: P10+ death tests and tail coverage items.
*/

#include "TestHelper.h"
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

namespace zmeya_coverage
{

inline void ExpectHashMapStrIntMatchesModel(const zm::HashMap<zm::String, int32_t>& m, const std::unordered_map<std::string, int32_t>& model)
{
    for (const auto& kv : model)
    {
        EXPECT_TRUE(m.contains(kv.first.c_str()));
        EXPECT_EQ(m.find(kv.first.c_str(), int32_t(-99999)), kv.second);
    }
}

inline std::vector<std::string> SortedStringSet(const zm::HashSet<zm::String>& hs)
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

} // namespace zmeya_coverage

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
