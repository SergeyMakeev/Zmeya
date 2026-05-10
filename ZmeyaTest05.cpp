#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct StringTestRoot
{
    zm::String str1;
    zm::String str2;
    zm::String str3;
    zm::String str4;
    zm::String str5;
    zm::String str6;
    zm::Array<zm::String> strArr1;
    zm::Array<zm::String> strArr2;
    zm::Array<zm::String> strArr3;
    zm::Array<zm::String> strArr4;
};

static void validate(const StringTestRoot* root)
{
    EXPECT_STREQ(root->str1.c_str(), "Hello World - This is a very long test string. Expected 1000000 instances");
    EXPECT_STREQ(root->str2.c_str(), "Hello World 2");
    EXPECT_STREQ(root->str3.c_str(), "Hello W");
    EXPECT_STREQ(root->str4.c_str(), "Hello World - This is a very long test string. Expected 1000000 instances");
    EXPECT_STREQ(root->str5.c_str(), "Hello World 2");
    EXPECT_STREQ(root->str6.c_str(), "");

    EXPECT_TRUE(root->str1 == root->str1);
    EXPECT_TRUE(root->str2 == root->str5);
    EXPECT_TRUE(root->str1 == root->str4);
    EXPECT_FALSE(root->str1 == root->str2);
    EXPECT_FALSE(root->str1 == root->str3);
    EXPECT_FALSE(root->str1 == root->str5);

    EXPECT_EQ(root->strArr1.size(), std::size_t(4));
    EXPECT_EQ(root->strArr1[0], "first");
    EXPECT_EQ(root->strArr1[1], "second");
    EXPECT_EQ(root->strArr1[2], "third");
    EXPECT_EQ(root->strArr1[3], "fourth");

    /*
    EXPECT_EQ(root->strArr2.size(), std::size_t(3));
    EXPECT_EQ(root->strArr2[0], "one");
    EXPECT_EQ(root->strArr2[1], "two");
    EXPECT_EQ(root->strArr2[2], "three");

    EXPECT_EQ(root->strArr3.size(), std::size_t(2));
    EXPECT_EQ(root->strArr3[0], "hello");
    EXPECT_EQ(root->strArr3[1], "world");

    EXPECT_EQ(root->strArr4.size(), std::size_t(10)); // Updated for debugging
    for (const zm::String& s : root->strArr4)
    {
        EXPECT_EQ(s, root->str1);
    }
    */
}

TEST(ZmeyaTestSuite, StringTest_Debug)
{
    std::unique_ptr<zm::Builder<StringTestRoot>> builder = zm::Builder<StringTestRoot>::create();
    zm::ScopedBuilder scope(builder.get());

    StringTestRoot* root = builder->getRoot();

    EXPECT_TRUE(builder->contains_pointer(root));
    EXPECT_TRUE(builder->contains_pointer(&root->strArr1));

    zm::assign(root->str1, "test");

    std::vector<std::string> arr1 = {"first"};
    zm::assign(root->strArr1, arr1);
}

TEST(ZmeyaTestSuite, StringTest)
{
    std::vector<char> bytesCopy;
    {
        std::unique_ptr<zm::Builder<StringTestRoot>> builder = zm::Builder<StringTestRoot>::create(1024 * 1024);
        zm::ScopedBuilder scope(builder.get());

        zm::Builder<StringTestRoot>* b = builder.get();
        StringTestRoot* root = b->getRoot();

        zm::assign(root->str1, "Hello World - This is a very long test string. Expected 1000000 instances");

        std::string testStr("Hello World 2");
        zm::assign(root->str2, testStr);

        std::string substr("Hello World 3", 7);
        zm::assign(root->str3, substr);

        zm::assign(root->str4, "Hello World - This is a very long test string. Expected 1000000 instances");

        zm::assign(root->str5, "Hello World 2");

        std::vector<std::string> arr1 = {"first", "second", "third", "fourth"};
        zm::assign(root->strArr1, arr1);

        std::vector<std::string> arr2 = {"one", "two", "three"};
        zm::assign(root->strArr2, arr2);

        std::vector<std::string> arr3 = {"hello", "world"};
        zm::assign(root->strArr3, arr3);

        size_t numStrings = 10;
        std::vector<std::string> arr4(numStrings, "Hello World - This is a very long test string. Expected 1000000 instances");
        zm::assign(root->strArr4, arr4);

        validate(root);

        zm::Span<char> bytes = builder->finalize();

        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const StringTestRoot* rootCopy = (const StringTestRoot*)(bytesCopy.data());

    validate(rootCopy);
}
