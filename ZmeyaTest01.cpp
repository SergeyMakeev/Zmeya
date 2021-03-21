#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

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
    // create blob
    std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create();

    // allocate structure
    zm::BlobPtr<SimpleTestRoot> root = blobBuilder->allocate<SimpleTestRoot>();

    // fill with data
    root->a = 13.0f;
    root->b = 1979;
    root->c = 6;
    root->d = -9;
    for (size_t i = 0; i < std::size(root->arr); i++)
    {
        root->arr[i] = uint32_t(i + 3);
    }

    validate(root.get());

    // finalize blob
    zm::Span<char> bytes = blobBuilder->finalize();

    // copy resulting bytes
    std::vector<char> bytesCopy = utils::copyBytes(bytes);

    // "deserialize" and test results
    const SimpleTestRoot* rootCopy = (const SimpleTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
