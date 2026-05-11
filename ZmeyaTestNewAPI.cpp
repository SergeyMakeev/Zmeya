#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#include <cstring>
#include <dbghelp.h>
#include <iostream>
#include <memory>
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

Test zm::write_blob / zm::BlobWriter deep-copy adapters for STL-shaped RHS.

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

TEST(ZmeyaTestSuite, NewBuilderAPI_BasicTypes)
{
    std::vector<char> blob = zm::write_blob<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            TestRoot* root = w.root();
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

TEST(ZmeyaTestSuite, NewBuilderAPI_NestedTypes)
{
    std::vector<char> blob = zm::write_blob<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            TestRoot* root = w.root();
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

static void FillBasicTestRoot(TestRoot* root)
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

TEST(ZmeyaTestSuite, NewBuilderAPI_ExplicitBuilderAssignOverload)
{
    std::unique_ptr<zm::detail::Builder<TestRoot>> builder = zm::detail::Builder<TestRoot>::create(8192);
    TestRoot* root = builder->getRoot();

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

    std::vector<char> blobB = zm::write_blob<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            FillBasicTestRoot(w.root());
        });

    ASSERT_EQ(blobA.size(), blobB.size());
    EXPECT_EQ(std::memcmp(blobA.data(), blobB.data(), blobA.size()), 0);
}

TEST(ZmeyaTestSuite, NewBuilderAPI_ForcedReallocGoldenMatchesLargeReserve)
{
    constexpr size_t kTinyReserve = 32;

    std::vector<char> golden = zm::write_blob<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            FillBasicTestRootFresh(w);
        },
        1024 * 1024,
        4);

    std::vector<char> stressed = zm::write_blob<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            FillBasicTestRootFresh(w);
        },
        kTinyReserve,
        4);

    ASSERT_EQ(golden.size(), stressed.size());
    EXPECT_EQ(std::memcmp(golden.data(), stressed.data(), golden.size()), 0);
}

TEST(ZmeyaTestSuite, NewBuilderAPI_BlobWriterBuilderBaseAccessor)
{
    zm::write_blob<TestRoot>(
        [](zm::BlobWriter<TestRoot>& w)
        {
            zm::detail::BuilderBase* bb = w.builder_base();
            EXPECT_NE(bb, nullptr);
            EXPECT_TRUE(bb->contains_pointer(w.root()));
        });
}
