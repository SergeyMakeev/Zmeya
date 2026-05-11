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
}

TEST(ZmeyaTestSuite, StringTest_Debug)
{
    zm::build<StringTestRoot>([](zm::BuildSession<StringTestRoot>& session)
                              {
                                  StringTestRoot* root = session.root();

                                  EXPECT_TRUE(session.contains_pointer(root));
                                  EXPECT_TRUE(session.contains_pointer(&root->strArr1));

                                  root->str1 = "test";

                                  std::vector<std::string> arr1 = {"first"};
                                  root->strArr1 = arr1;
                              });
}

TEST(ZmeyaTestSuite, StringTest)
{
    std::vector<char> bytesCopy = zm::build<StringTestRoot>(
        [](zm::BuildSession<StringTestRoot>& session)
        {
            StringTestRoot* root = session.root();

            root->str1 = "Hello World - This is a very long test string. Expected 1000000 instances";

            std::string testStr("Hello World 2");
            root->str2 = testStr;

            std::string substr("Hello World 3", 7);
            root->str3 = substr;

            root->str4 = "Hello World - This is a very long test string. Expected 1000000 instances";

            root->str5 = "Hello World 2";

            std::vector<std::string> arr1 = {"first", "second", "third", "fourth"};
            root->strArr1 = arr1;

            std::vector<std::string> arr2 = {"one", "two", "three"};
            root->strArr2 = arr2;

            std::vector<std::string> arr3 = {"hello", "world"};
            root->strArr3 = arr3;

            size_t numStrings = 10;
            std::vector<std::string> arr4(numStrings, "Hello World - This is a very long test string. Expected 1000000 instances");
            root->strArr4 = arr4;

            validate(root);
        },
        1024 * 1024);

    const StringTestRoot* rootCopy = (const StringTestRoot*)(bytesCopy.data());

    validate(rootCopy);
}
