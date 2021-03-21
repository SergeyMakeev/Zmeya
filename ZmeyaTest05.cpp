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

    EXPECT_EQ(root->strArr2.size(), std::size_t(3));
    EXPECT_EQ(root->strArr2[0], "one");
    EXPECT_EQ(root->strArr2[1], "two");
    EXPECT_EQ(root->strArr2[2], "three");

    EXPECT_EQ(root->strArr3.size(), std::size_t(2));
    EXPECT_EQ(root->strArr3[0], "hello");
    EXPECT_EQ(root->strArr3[1], "world");

    EXPECT_EQ(root->strArr4.size(), std::size_t(1000000));
    for (const zm::String& s : root->strArr4)
    {
        EXPECT_EQ(s, root->str1);
    }
}

TEST(ZmeyaTestSuite, StringTest)
{
    std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create();
    zm::BlobPtr<StringTestRoot> root = blobBuilder->allocate<StringTestRoot>();

    // assign from null-terminated c string (74 bytes long)
    blobBuilder->copyTo(root->str1, "Hello World - This is a very long test string. Expected 1000000 instances");

    // assign from std::string
    std::string testStr("Hello World 2");
    blobBuilder->copyTo(root->str2, testStr);

    // assign range
    blobBuilder->copyTo(root->str3, "Hello World 3", 7);

    // assign existing string (do not introduce extra copy!)
    blobBuilder->referTo(root->str4, root->str1);

    blobBuilder->copyTo(root->str5, "Hello World 2");

    std::vector<std::string> arr1 = {"first", "second", "third", "fourth"};
    blobBuilder->copyTo(root->strArr1, arr1);
    blobBuilder->copyTo(root->strArr2, {"one", "two", "three"});

    std::array<const char*, 2> arr3 = {"hello", "world"};
    blobBuilder->copyTo(root->strArr3, arr3);

    // 1,000,000 strings 74 bytes each
    // since we are assigning already existing string the string body will not be copied (only offset will be stored)
    size_t numStrings = 1000000;
    blobBuilder->resizeArray(root->strArr4, numStrings);
    for (size_t i = 0; i < root->strArr4.size(); i++)
    {
        blobBuilder->referTo(root->strArr4[i], root->str1);
    }

    validate(root.get());

    zm::Span<char> bytes = blobBuilder->finalize();
    EXPECT_LE(bytes.size, std::size_t(4000500));

    std::vector<char> bytesCopy = utils::copyBytes(bytes);

    const StringTestRoot* rootCopy = (const StringTestRoot*)(bytesCopy.data());

    validate(rootCopy);
}
