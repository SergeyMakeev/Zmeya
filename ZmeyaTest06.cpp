#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct HashSetTestRoot
{
    zm::HashSet<int32_t> set1;
    zm::HashSet<int32_t> set2;
    zm::HashSet<int32_t> set3;
    zm::HashSet<zm::String> strSet1;
    zm::HashSet<zm::String> strSet2;
};

static void validate(const HashSetTestRoot* root)
{
    EXPECT_EQ(root->set1.size(), std::size_t(5));
    EXPECT_TRUE(root->set1.contains(99));
    EXPECT_TRUE(root->set1.contains(5));
    EXPECT_TRUE(root->set1.contains(7));
    EXPECT_TRUE(root->set1.contains(3));
    EXPECT_TRUE(root->set1.contains(11));
    EXPECT_FALSE(root->set1.contains(1));
    EXPECT_FALSE(root->set1.contains(6));
    EXPECT_FALSE(root->set1.contains(15));
    EXPECT_FALSE(root->set1.contains(88));

    EXPECT_EQ(root->set2.size(), std::size_t(7));
    EXPECT_TRUE(root->set2.contains(1));
    EXPECT_TRUE(root->set2.contains(2));
    EXPECT_TRUE(root->set2.contains(3));
    EXPECT_TRUE(root->set2.contains(4));
    EXPECT_TRUE(root->set2.contains(0));
    EXPECT_TRUE(root->set2.contains(99));
    EXPECT_TRUE(root->set2.contains(6));
    EXPECT_FALSE(root->set2.contains(7));
    EXPECT_FALSE(root->set2.contains(-2));
    EXPECT_FALSE(root->set2.contains(11));

    EXPECT_EQ(root->set3.size(), std::size_t(0));
    EXPECT_FALSE(root->set3.contains(1));
    EXPECT_FALSE(root->set3.contains(2));
    EXPECT_FALSE(root->set3.contains(99));
    EXPECT_FALSE(root->set3.contains(120));

    EXPECT_EQ(root->strSet1.size(), std::size_t(6));
    EXPECT_FALSE(root->strSet1.contains("zero"));
    EXPECT_TRUE(root->strSet1.contains("one"));
    EXPECT_TRUE(root->strSet1.contains("two"));
    EXPECT_TRUE(root->strSet1.contains("three"));
    EXPECT_TRUE(root->strSet1.contains("four"));
    EXPECT_TRUE(root->strSet1.contains("123456"));
    EXPECT_TRUE(root->strSet1.contains("1234567"));
    EXPECT_FALSE(root->strSet1.contains("five"));
    EXPECT_FALSE(root->strSet1.contains("six"));
    EXPECT_FALSE(root->strSet1.contains("seven"));

    EXPECT_EQ(root->strSet2.size(), std::size_t(5));
    EXPECT_TRUE(root->strSet2.contains("five"));
    EXPECT_TRUE(root->strSet2.contains("six"));
    EXPECT_TRUE(root->strSet2.contains("seven"));
    EXPECT_TRUE(root->strSet2.contains("eight"));
    EXPECT_TRUE(root->strSet2.contains("this-is-a-very-very-long-key-to-test-hasher"));
    EXPECT_FALSE(root->strSet2.contains("one"));
    EXPECT_FALSE(root->strSet2.contains("two"));
    EXPECT_FALSE(root->strSet2.contains("three"));
    EXPECT_FALSE(root->strSet2.contains("four"));
}

TEST(ZmeyaTestSuite, HashSetTest)
{
    std::vector<char> bytesCopy;
    {
        std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create(1);
        zm::BlobPtr<HashSetTestRoot> root = blobBuilder->allocate<HashSetTestRoot>();

        // assign from std::unordered_set
        std::unordered_set<int> testSet1 = {5, 7, 3, 11, 99};
        blobBuilder->copyTo(root->set1, testSet1);

        // assign from std::initializer_list
        blobBuilder->copyTo(root->set2, {1, 2, 3, 4, 0, 99, 6});

        // assign from std::unordered_set of strings
        std::unordered_set<std::string> strSet1 = {"one", "two", "three", "four", "123456", "1234567"};
        blobBuilder->copyTo(root->strSet1, strSet1);

        // assign from std::initializer_list of strings
        blobBuilder->copyTo(root->strSet2, {"five", "six", "seven", "eight", "this-is-a-very-very-long-key-to-test-hasher"});

        validate(root.get());

        zm::Span<char> bytes = blobBuilder->finalize();
        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const HashSetTestRoot* rootCopy = (const HashSetTestRoot*)(bytesCopy.data());

    validate(rootCopy);
}
