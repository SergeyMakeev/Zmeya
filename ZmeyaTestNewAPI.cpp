#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

namespace zm
{
void onAssertionFailed(const char* expression, const char* srcFile, unsigned int srcLine)
{
    printf("Assertion failed: %s, file: %s, line: %u\n", expression, srcFile, srcLine);
    int a = 0;
    a = 7;
}
} // namespace zm

/*

Test the new simplified Builder API with deep-copy adapters

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
    std::shared_ptr<zm::Builder> _builder = zm::Builder::create();
    zm::ScopedBuilder scope(_builder.get());

    zm::Builder* builder = zm::detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);

    // Allocate root
    TestRoot* root = builder->allocate_root<TestRoot>();

    // Test basic string assignment
    std::string srcDesc = "Test description";
    zm::assign(root->description, srcDesc);

    // Test array of primitives
    std::vector<int32_t> srcInts = {1, 2, 3, 4, 5};
    zm::assign(root->intArray, srcInts);

    // Test array of strings
    std::vector<std::string> srcStrings = {"hello", "world", "test"};
    zm::assign(root->stringArray, srcStrings);

    // Test HashMap
    std::unordered_map<std::string, int32_t> srcMap = {{"one", 1}, {"two", 2}, {"three", 3}};
    zm::assign(root->hashMap, srcMap);

    // Test HashSet
    std::unordered_set<std::string> srcSet = {"alpha", "beta", "gamma"};
    zm::assign(root->hashSet, srcSet);

    // Get result
    zm::Span<char> bytes = builder->finalize();

    // Validate the serialized data
    const TestRoot* fileRoot = reinterpret_cast<const TestRoot*>(bytes.data);

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
    std::shared_ptr<zm::Builder> _builder = zm::Builder::create();
    zm::ScopedBuilder scope(_builder.get());

    zm::Builder* builder = zm::detail::get_global_builder();
    ZMEYA_ASSERT(builder != nullptr);
    // Allocate root
    TestRoot* root = builder->allocate_root<TestRoot>();

    // Test nested array conversion
    std::vector<std::vector<std::string>> srcNested = {{"a", "b", "c"}, {"x", "y"}, {"hello", "world", "nested", "test"}};
    zm::assign(root->nestedArray, srcNested);

    // Get result
    zm::Span<char> bytes = builder->finalize();

    // Validate nested structure
    const TestRoot* fileRoot = reinterpret_cast<const TestRoot*>(bytes.data);

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
