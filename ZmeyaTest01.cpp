#include "gtest/gtest.h"

#include "TestHelper.h"
#include "Zmeya.h"

struct SimpleTestRoot
{
    float a;
    uint32_t b;
    uint16_t c;
    int8_t d;
    uint32_t arr[32];
};

static void validate(const SimpleTestRoot* root)
{
    EXPECT_FLOAT_EQ(root->a, 13.0f);
    EXPECT_EQ(root->b, 1979u);
    EXPECT_EQ(root->c, 6);
    EXPECT_EQ(root->d, -9);
    for (size_t i = 0; i < 32; i++)
    {
        EXPECT_EQ(root->arr[i], uint32_t(i + 3));
    }
}

TEST(ZmeyaTestSuite, SimpleTest)
{
    std::vector<char> bytesCopy = zm::write_blob<SimpleTestRoot>(
        [](zm::BlobWriter<SimpleTestRoot>& w)
        {
            SimpleTestRoot* root = w.root();
            root->a = 13.0f;
            root->b = 1979;
            root->c = 6;
            root->d = -9;
            for (size_t i = 0; i < 32; i++)
            {
                root->arr[i] = uint32_t(i + 3);
            }

            validate(root);
        });

    const SimpleTestRoot* rootCopy = (const SimpleTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}

struct Desc
{
    zm::String name;
    float v1;
    uint32_t v2;
    
    // Assignment operator for automatic conversion from TempDesc
    template<typename TempType>
    Desc& operator=(const TempType& temp)
    {
        name = temp.name;
        v1 = temp.v1;
        v2 = temp.v2;
        return *this;
    }
};

struct TestRoot
{
    zm::Array<Desc> arr;
};

TEST(ZmeyaTestSuite, SimpleTest2)
{
    const std::vector<std::string> names = {"apple",   "banana",  "orange",  "castle",  "dragon",  "flower",  "guitar",
                                            "hockey",  "island",  "jungle",  "kingdom", "library", "monster", "notable",
                                            "oceanic", "painter", "quarter", "rescue",  "seventh", "trivial", "umbrella",
                                            "village", "warrior", "xenial",  "yonder",  "zephyr"};

    struct TempDesc
    {
        std::string name;
        float v1;
        uint32_t v2;
    };

    std::vector<char> blob = zm::write_blob<TestRoot>(
        [&](zm::BlobWriter<TestRoot>& w)
        {
            TestRoot* root = w.root();
            std::vector<TempDesc> tempDescs;
            tempDescs.reserve(names.size());

            for (size_t i = 0; i < names.size(); i++)
            {
                TempDesc desc{};
                desc.name = names[i];
                desc.v1 = (float)(i);
                desc.v2 = (uint32_t)(i);
                tempDescs.push_back(desc);
            }

            root->arr = tempDescs;

            EXPECT_EQ(root->arr.size(), names.size());

            for (size_t i = 0; i < names.size(); i++)
            {
                const char* s1 = root->arr[i].name.c_str();
                const char* s2 = names[i].c_str();
                EXPECT_STREQ(s1, s2);
                EXPECT_FLOAT_EQ(root->arr[i].v1, (float)(i));
                EXPECT_EQ(root->arr[i].v2, (uint32_t)(i));
            }
        });

    // validate
    const TestRoot* rootCopy = (const TestRoot*)(blob.data());
    EXPECT_EQ(rootCopy->arr.size(), names.size());

    for (size_t i = 0; i < names.size(); i++)
    {
        const Desc& desc = rootCopy->arr[i];
        EXPECT_STREQ(desc.name.c_str(), names[i].c_str());
        EXPECT_FLOAT_EQ(desc.v1, (float)(i));
        EXPECT_EQ(desc.v2, (uint32_t)(i));
    }
}