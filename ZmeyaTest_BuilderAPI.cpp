#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>
#include <dbghelp.h>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <windows.h>

#pragma comment(lib, "dbghelp.lib")

void PrintStackTrace()
{
    void* stack[64];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);

    USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);

    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (USHORT i = 0; i < frames; ++i)
    {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        std::cout << frames - i - 1 << ": " << symbol->Name << " - 0x" << std::hex << symbol->Address << std::dec << "\n";
    }

    free(symbol);
}

namespace zm
{
void onAssertionFailed(const char* expression, const char* srcFile, unsigned int srcLine)
{
    printf("Assertion failed: %s, file: %s, line: %u\n", expression, srcFile, srcLine);
    PrintStackTrace();
    std::abort();
}
} // namespace zm

/*

Test zm::write_scope / zm::BlobWriter deep-copy adapters for STL-shaped RHS.

*/

struct TestRoot
{
    zm::String description;
    zm::Array<zm::String> stringArray;
    zm::Array<int32_t> intArray;
    zm::HashMap<zm::String, int32_t> hashMap;
    zm::HashSet<zm::String> hashSet;
    zm::Array<zm::Array<zm::String>> nestedArray;
};

struct ArenaRefStressNode
{
    int32_t u = 0;
    int32_t v = 0;
};

struct ArenaRefStressRoot
{
    zm::String churn;
    zm::Pointer<ArenaRefStressNode> left;
    zm::Pointer<ArenaRefStressNode> right;
};

// Verifies operator= from STL strings, vectors, map, and set into a root blob round-trip on read.
TEST(ZmeyaTestSuite, NewBuilderAPI_BasicTypes)
{
    zm::BlobBuffer blob = zm::write_scope<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            zm::ArenaRef<TestRoot> root = w.root();
            std::string srcDesc = "Test description";
            root->description = srcDesc;

            std::vector<int32_t> srcInts = {1, 2, 3, 4, 5};
            root->intArray = srcInts;

            std::vector<std::string> srcStrings = {"hello", "world", "test"};
            root->stringArray = srcStrings;

            std::unordered_map<std::string, int32_t> srcMap = {{"one", 1}, {"two", 2}, {"three", 3}};
            root->hashMap = srcMap;

            std::unordered_set<std::string> srcSet = {"alpha", "beta", "gamma"};
            root->hashSet = srcSet;
        });

    const TestRoot* fileRoot = reinterpret_cast<const TestRoot*>(blob.data());

    EXPECT_EQ(fileRoot->description, "Test description");

    EXPECT_EQ(fileRoot->intArray.size(), 5);
    EXPECT_EQ(fileRoot->intArray[0], 1);
    EXPECT_EQ(fileRoot->intArray[4], 5);

    EXPECT_EQ(fileRoot->stringArray.size(), 3);
    EXPECT_EQ(fileRoot->stringArray[0], "hello");
    EXPECT_EQ(fileRoot->stringArray[1], "world");
    EXPECT_EQ(fileRoot->stringArray[2], "test");

    EXPECT_EQ(fileRoot->hashMap.size(), 3);
    EXPECT_EQ(*fileRoot->hashMap.find("one"), 1);
    EXPECT_EQ(*fileRoot->hashMap.find("two"), 2);
    EXPECT_EQ(*fileRoot->hashMap.find("three"), 3);

    EXPECT_EQ(fileRoot->hashSet.size(), 3);
    EXPECT_TRUE(fileRoot->hashSet.contains("alpha"));
    EXPECT_TRUE(fileRoot->hashSet.contains("beta"));
    EXPECT_TRUE(fileRoot->hashSet.contains("gamma"));
}

// Verifies nested vector-of-vector-of-string assigns into zm::Array<zm::Array<zm::String>> and reads back correctly.
TEST(ZmeyaTestSuite, NewBuilderAPI_NestedTypes)
{
    zm::BlobBuffer blob = zm::write_scope<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            zm::ArenaRef<TestRoot> root = w.root();
            std::vector<std::vector<std::string>> srcNested = {{"a", "b", "c"}, {"x", "y"}, {"hello", "world", "nested", "test"}};
            root->nestedArray = srcNested;
        });

    const TestRoot* fileRoot = reinterpret_cast<const TestRoot*>(blob.data());

    EXPECT_EQ(fileRoot->nestedArray.size(), 3);

    EXPECT_EQ(fileRoot->nestedArray[0].size(), 3);
    EXPECT_EQ(fileRoot->nestedArray[0][0], "a");
    EXPECT_EQ(fileRoot->nestedArray[0][1], "b");
    EXPECT_EQ(fileRoot->nestedArray[0][2], "c");

    EXPECT_EQ(fileRoot->nestedArray[1].size(), 2);
    EXPECT_EQ(fileRoot->nestedArray[1][0], "x");
    EXPECT_EQ(fileRoot->nestedArray[1][1], "y");

    EXPECT_EQ(fileRoot->nestedArray[2].size(), 4);
    EXPECT_EQ(fileRoot->nestedArray[2][0], "hello");
    EXPECT_EQ(fileRoot->nestedArray[2][1], "world");
    EXPECT_EQ(fileRoot->nestedArray[2][2], "nested");
    EXPECT_EQ(fileRoot->nestedArray[2][3], "test");
}

static void FillBasicTestRoot(const zm::ArenaRef<TestRoot>& root)
{
    root->description = "Test description";

    std::vector<int32_t> srcInts = {1, 2, 3, 4, 5};
    root->intArray = srcInts;

    std::vector<std::string> srcStrings = {"hello", "world", "test"};
    root->stringArray = srcStrings;

    std::unordered_map<std::string, int32_t> srcMap = {{"one", 1}, {"two", 2}, {"three", 3}};
    root->hashMap = srcMap;

    std::unordered_set<std::string> srcSet = {"alpha", "beta", "gamma"};
    root->hashSet = srcSet;
}

template <typename BlobWriterT>
static void FillBasicTestRootFresh(BlobWriterT& w)
{
    w.root()->description = "Test description";

    const std::vector<int32_t> srcInts = {1, 2, 3, 4, 5};
    w.root()->intArray = srcInts;

    const std::vector<std::string> srcStrings = {"hello", "world", "test"};
    w.root()->stringArray = srcStrings;

    const std::unordered_map<std::string, int32_t> srcMap = {{"one", 1}, {"two", 2}, {"three", 3}};
    w.root()->hashMap = srcMap;

    const std::unordered_set<std::string> srcSet = {"alpha", "beta", "gamma"};
    w.root()->hashSet = srcSet;
}

static std::unordered_map<std::string, int32_t> ReadLogicalStringIntMap(const zm::HashMap<zm::String, int32_t>& m)
{
    std::unordered_map<std::string, int32_t> out;
    out.reserve(m.size());
    for (const auto& kv : m)
    {
        out.emplace(std::string(kv.first.c_str()), kv.second);
    }
    return out;
}

static std::vector<std::string> ReadSortedStringSet(const zm::HashSet<zm::String>& hs)
{
    std::vector<std::string> v;
    v.reserve(hs.size());
    for (const zm::String& s : hs)
    {
        v.emplace_back(s.c_str());
    }
    std::sort(v.begin(), v.end());
    return v;
}

static void ExpectUnorderedStringIntMapsEqual(const std::unordered_map<std::string, int32_t>& a,
    const std::unordered_map<std::string, int32_t>& b)
{
    ASSERT_EQ(a.size(), b.size());
    for (const auto& kv : a)
    {
        const auto it = b.find(kv.first);
        ASSERT_NE(it, b.end()) << kv.first;
        EXPECT_EQ(it->second, kv.second);
    }
}

template <typename A, typename B>
static void ExpectTestRootLogicalEqual(const A& a, const B& b)
{
    ASSERT_EQ(a.size(), b.size());
    const TestRoot* ra = reinterpret_cast<const TestRoot*>(a.data());
    const TestRoot* rb = reinterpret_cast<const TestRoot*>(b.data());

    EXPECT_STREQ(ra->description.c_str(), rb->description.c_str());

    ASSERT_EQ(ra->intArray.size(), rb->intArray.size());
    for (size_t i = 0; i < ra->intArray.size(); ++i)
    {
        EXPECT_EQ(ra->intArray[i], rb->intArray[i]);
    }

    ASSERT_EQ(ra->stringArray.size(), rb->stringArray.size());
    for (size_t i = 0; i < ra->stringArray.size(); ++i)
    {
        EXPECT_STREQ(ra->stringArray[i].c_str(), rb->stringArray[i].c_str());
    }

    ExpectUnorderedStringIntMapsEqual(ReadLogicalStringIntMap(ra->hashMap), ReadLogicalStringIntMap(rb->hashMap));
    EXPECT_EQ(ReadSortedStringSet(ra->hashSet), ReadSortedStringSet(rb->hashSet));

    ASSERT_EQ(ra->nestedArray.size(), rb->nestedArray.size());
    for (size_t i = 0; i < ra->nestedArray.size(); ++i)
    {
        ASSERT_EQ(ra->nestedArray[i].size(), rb->nestedArray[i].size());
        for (size_t j = 0; j < ra->nestedArray[i].size(); ++j)
        {
            EXPECT_STREQ(ra->nestedArray[i][j].c_str(), rb->nestedArray[i][j].c_str());
        }
    }
}

// Verifies zm::assign(BuilderBase&, ...) matches write_scope logical content for the same TestRoot fields.
TEST(ZmeyaTestSuite, NewBuilderAPI_ExplicitBuilderAssignOverload)
{
    std::unique_ptr<zm::detail::Builder<TestRoot>> builder = zm::detail::Builder<TestRoot>::create();
    zm::ArenaRef<TestRoot> root = builder->getRoot();

    zm::assign(*builder, root->description, std::string("Test description"));

    const std::vector<int32_t> srcInts = {1, 2, 3, 4, 5};
    zm::assign(*builder, root->intArray, srcInts);

    const std::vector<std::string> srcStrings = {"hello", "world", "test"};
    zm::assign(*builder, root->stringArray, srcStrings);

    const std::unordered_map<std::string, int32_t> srcMap = {{"one", 1}, {"two", 2}, {"three", 3}};
    zm::assign(*builder, root->hashMap, srcMap);

    const std::unordered_set<std::string> srcSet = {"alpha", "beta", "gamma"};
    zm::assign(*builder, root->hashSet, srcSet);

    zm::Span<char> span = builder->finalize(4);
    std::vector<char> blobA(span.data, span.data + span.size);

    zm::BlobBuffer blobB = zm::write_scope<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            FillBasicTestRoot(w.root());
        });

    ExpectTestRootLogicalEqual(blobA, blobB);
}

// Verifies default arena sizing matches a deliberately tiny arena (realloc + patch correctness).
TEST(ZmeyaTestSuite, NewBuilderAPI_ForcedReallocGoldenMatchesDefaultArena)
{
    constexpr size_t kTinyArenaBytes = 32;

    zm::BlobBuffer golden = zm::write_scope<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            FillBasicTestRootFresh(w);
        });

    zm::BlobBuffer stressed = zmeya_test::write_scope_stressed<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            FillBasicTestRootFresh(w);
        },
        kTinyArenaBytes,
        4);

    ExpectTestRootLogicalEqual(golden, stressed);
}

// One captured ArenaRef<TestRoot> must stay valid across many arena reallocations (same logical blob as golden).
TEST(ZmeyaTestSuite, NewBuilderAPI_CachedRootArenaRefSurvivesStressedReallocMatchesGolden)
{
    constexpr size_t kTinyArenaBytes = 32;

    zm::BlobBuffer golden = zm::write_scope<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            FillBasicTestRoot(w.root());
        });

    zm::BlobBuffer stressed = zmeya_test::write_scope_stressed<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            const zm::ArenaRef<TestRoot> root = w.root();
            FillBasicTestRoot(root);
        },
        kTinyArenaBytes,
        4);

    ExpectTestRootLogicalEqual(golden, stressed);
}

// Cached ArenaRef from allocate() plus root ref: interleaved string growth forces realloc; refs must keep resolving.
TEST(ZmeyaTestSuite, NewBuilderAPI_CachedAllocateArenaRefsSurviveInterleavedArenaGrowth)
{
    constexpr size_t kTinyArenaBytes = 16;

    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<ArenaRefStressRoot>(
        [](zm::BlobWriter<ArenaRefStressRoot>& w)
        {
            zm::ArenaRef<ArenaRefStressRoot> root = w.root();
            zm::ArenaRef<ArenaRefStressNode> L = w.allocate<ArenaRefStressNode>();
            zm::ArenaRef<ArenaRefStressNode> R = w.allocate<ArenaRefStressNode>();
            L->u = 111;
            L->v = 222;
            R->u = 333;
            R->v = 444;

            for (int i = 0; i < 500; ++i)
            {
                root->churn += "Z";
            }

            EXPECT_EQ(L->u, 111);
            EXPECT_EQ(R->v, 444);
            L->v = 999;
            R->u = 1000;

            for (int i = 0; i < 300; ++i)
            {
                root->churn += "yy";
            }

            EXPECT_EQ(L->v, 999);
            EXPECT_EQ(R->u, 1000);

            root->left = L.transient_ptr();
            root->right = R.transient_ptr();

            for (int i = 0; i < 200; ++i)
            {
                root->churn += "pad";
            }

            EXPECT_EQ(L->u, 111);
            EXPECT_EQ(R->v, 444);
        },
        kTinyArenaBytes,
        4);

    const ArenaRefStressRoot* rr = reinterpret_cast<const ArenaRefStressRoot*>(blob.data());
    ASSERT_NE(rr->left.get(), nullptr);
    ASSERT_NE(rr->right.get(), nullptr);
    EXPECT_EQ(rr->left->u, 111);
    EXPECT_EQ(rr->left->v, 999);
    EXPECT_EQ(rr->right->u, 1000);
    EXPECT_EQ(rr->right->v, 444);
    EXPECT_GT(std::strlen(rr->churn.c_str()), 1500u);
}

// Verifies BlobWriter exposes a live builder_base and that the root pointer lies inside the builder arena.
TEST(ZmeyaTestSuite, NewBuilderAPI_BlobWriterBuilderBaseAccessor)
{
    zm::write_scope<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            zm::detail::BuilderBase* bb = w.builder_base();
            EXPECT_NE(bb, nullptr);
            EXPECT_TRUE(bb->contains_pointer(w.root().transient_ptr()));
        });
}
