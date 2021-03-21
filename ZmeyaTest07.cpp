#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct HashMapTestRoot
{
    zm::HashMap<int32_t, float> hashMap1;
    zm::HashMap<int32_t, float> hashMap2;
    zm::HashMap<zm::String, float> strHashMap1;
    zm::HashMap<zm::String, float> strHashMap2;
    zm::HashMap<int32_t, zm::String> strHashMap3;
    zm::HashMap<int32_t, zm::String> strHashMap4;
    zm::HashMap<zm::String, zm::String> strHashMap5;
    zm::HashMap<zm::String, zm::String> strHashMap6;
};

static void validate(const HashMapTestRoot* root)
{
    EXPECT_EQ(root->hashMap1.size(), 5);
    EXPECT_FLOAT_EQ(root->hashMap1.find(3, -1.0f), 7.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(4, -1.0f), 17.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(9, -1.0f), 79.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(11, -1.0f), 13.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(12, -1.0f), -1.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(77, 13.0f), 13.0f);
    EXPECT_EQ(root->hashMap1.find(99), nullptr);
    EXPECT_NE(root->hashMap1.find(3), nullptr);
    EXPECT_EQ(root->hashMap1.contains(99), false);
    EXPECT_EQ(root->hashMap1.contains(3), true);

    EXPECT_EQ(root->hashMap2.size(), 3);
    EXPECT_FLOAT_EQ(root->hashMap2.find(1, 0.0f), -1.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(2, 0.0f), -2.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(3, 0.0f), -3.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(4, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(5, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(6, 0.0f), 0.0f);

    EXPECT_EQ(root->strHashMap1.size(), 3);
    EXPECT_FLOAT_EQ(root->strHashMap1.find("one", 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(root->strHashMap1.find("two", 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(root->strHashMap1.find("three", 0.0f), 3.0f);
    EXPECT_FLOAT_EQ(root->strHashMap1.find("five", 0.0f), 0.0f);

    EXPECT_EQ(root->strHashMap2.size(), 2);
    EXPECT_FLOAT_EQ(root->strHashMap2.find("five", 0.0f), -5.0f);
    EXPECT_FLOAT_EQ(root->strHashMap2.find("six", 0.0f), -6.0f);
    EXPECT_FLOAT_EQ(root->strHashMap2.find("seven", 0.0f), 0.0f);

    EXPECT_EQ(root->strHashMap3.size(), 5);
    EXPECT_STREQ(root->strHashMap3.find(1, ""), "one");
    EXPECT_STREQ(root->strHashMap3.find(2, ""), "two");
    EXPECT_STREQ(root->strHashMap3.find(3, ""), "three");
    EXPECT_STREQ(root->strHashMap3.find(5, ""), "five");
    EXPECT_STREQ(root->strHashMap3.find(10, ""), "ten");
    EXPECT_STREQ(root->strHashMap3.find(13, nullptr), nullptr);

    EXPECT_EQ(root->strHashMap4.size(), 2);
    EXPECT_STREQ(root->strHashMap4.find(5, ""), "five");
    EXPECT_STREQ(root->strHashMap4.find(7, ""), "seven");
    EXPECT_STREQ(root->strHashMap4.find(6, nullptr), nullptr);

    EXPECT_EQ(root->strHashMap5.size(), 5);
    EXPECT_STREQ(root->strHashMap5.find("1", ""), "one");
    EXPECT_STREQ(root->strHashMap5.find("2", ""), "two");
    EXPECT_STREQ(root->strHashMap5.find("3", ""), "three");
    EXPECT_STREQ(root->strHashMap5.find("5", ""), "five");
    EXPECT_STREQ(root->strHashMap5.find("10", ""), "ten");
    EXPECT_STREQ(root->strHashMap5.find("13", nullptr), nullptr);

    EXPECT_EQ(root->strHashMap6.size(), 2);
    EXPECT_STREQ(root->strHashMap6.find("5", ""), "five");
    EXPECT_STREQ(root->strHashMap6.find("7", ""), "seven");
    EXPECT_STREQ(root->strHashMap6.find("6", nullptr), nullptr);
}

TEST(ZmeyaTestSuite, HashMapTest)
{
    std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create();
    zm::BlobPtr<HashMapTestRoot> root = blobBuilder->allocate<HashMapTestRoot>();

    // assign from std::unordered_map
    std::unordered_map<int, float> testMap = {{3, 7.0f}, {4, 17.0f}, {9, 79.0f}, {11, 13.0f}, {77, 13.0f}};
    blobBuilder->copyTo(root->hashMap1, testMap);

    // assign from std::initializer_list
    blobBuilder->copyTo(root->hashMap2, {{1, -1.0f}, {2, -2.0f}, {3, -3.0f}});

    // assign from std::unordered_map (string key)
    std::unordered_map<std::string, float> strMap1 = {{"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}};
    blobBuilder->copyTo(root->strHashMap1, strMap1);

    // assign from std::initializer_list (string key)
    blobBuilder->copyTo(root->strHashMap2, {{"five", -5.0f}, {"six", -6.0f}});

    // assign from std::unordered_map (string value)
    std::unordered_map<int, std::string> strMap3 = {{1, "one"}, {2, "two"}, {3, "three"}, {5, "five"}, {10, "ten"}};
    blobBuilder->copyTo(root->strHashMap3, strMap3);

    // assign from std::initializer_list (string value)
    blobBuilder->copyTo(root->strHashMap4, {{5, "five"}, {7, "seven"}});

    // assign from std::unordered_map (string key+value)
    std::unordered_map<std::string, std::string> strMap5 = {{"1", "one"}, {"2", "two"}, {"3", "three"}, {"5", "five"}, {"10", "ten"}};
    blobBuilder->copyTo(root->strHashMap5, strMap5);
    
    // assign from std::initializer_list (string key+value)
    blobBuilder->copyTo(root->strHashMap6, {{"5", "five"}, {"7", "seven"}});

    validate(root.get());

    zm::Span<char> bytes = blobBuilder->finalize();
    std::vector<char> bytesCopy = utils::copyBytes(bytes);
    const HashMapTestRoot* rootCopy = (const HashMapTestRoot*)(bytesCopy.data());

    validate(rootCopy);
}
