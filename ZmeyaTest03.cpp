#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct Payload
{
    float a;
    uint32_t b;

    Payload() = default;
    Payload(float _a, uint32_t _b)
        : a(_a)
        , b(_b)
    {
    }
};

struct ArrayTestRoot
{
    zm::Array<Payload> arr1;
    zm::Array<int32_t> arr2;
    zm::Array<float> arr3;
    zm::Array<zm::Array<float>> arr4;
    zm::Array<zm::Pointer<Payload>> arr5;
    zm::Array<zm::Array<uint32_t>> arr6;
};

static void validate(const ArrayTestRoot* root)
{
    EXPECT_EQ(root->arr1.size(), std::size_t(2));
    const Payload& item0 = root->arr1[0];
    EXPECT_FLOAT_EQ(item0.a, 1.3f);
    EXPECT_EQ(item0.b, 13u);
    const Payload& item1 = root->arr1[1];
    EXPECT_FLOAT_EQ(item1.a, 2.7f);
    EXPECT_EQ(item1.b, 27u);

    EXPECT_EQ(root->arr2.size(), std::size_t(6));
    EXPECT_EQ(root->arr2[0], 2);
    EXPECT_EQ(root->arr2[1], 4);
    EXPECT_EQ(root->arr2[2], 6);
    EXPECT_EQ(root->arr2[3], 10);
    EXPECT_EQ(root->arr2[4], 14);
    EXPECT_EQ(root->arr2[5], 32);

    EXPECT_EQ(root->arr3.size(), std::size_t(4));
    EXPECT_FLOAT_EQ(root->arr3[0], 67.0f);
    EXPECT_FLOAT_EQ(root->arr3[1], 82.0f);
    EXPECT_FLOAT_EQ(root->arr3[2], 11.0f);
    EXPECT_FLOAT_EQ(root->arr3[3], 54.0f);

    EXPECT_EQ(root->arr4.size(), std::size_t(4));
    EXPECT_EQ(root->arr4[0].size(), std::size_t(2));
    EXPECT_FLOAT_EQ(root->arr4[0][0], 1.2f);
    EXPECT_FLOAT_EQ(root->arr4[0][1], 2.3f);
    EXPECT_EQ(root->arr4[1].size(), std::size_t(3));
    EXPECT_FLOAT_EQ(root->arr4[1][0], 7.1f);
    EXPECT_FLOAT_EQ(root->arr4[1][1], 8.8f);
    EXPECT_FLOAT_EQ(root->arr4[1][2], 3.2f);
    EXPECT_EQ(root->arr4[2].size(), std::size_t(4));
    EXPECT_FLOAT_EQ(root->arr4[2][0], 16.0f);
    EXPECT_FLOAT_EQ(root->arr4[2][1], 12.0f);
    EXPECT_FLOAT_EQ(root->arr4[2][2], 99.5f);
    EXPECT_FLOAT_EQ(root->arr4[2][3], -143.0f);
    EXPECT_EQ(root->arr4[3].size(), std::size_t(1));
    EXPECT_FLOAT_EQ(root->arr4[3][0], -1.0f);

    EXPECT_EQ(root->arr5.size(), std::size_t(793));
    for (size_t i = 0; i < root->arr5.size(); i++)
    {
        EXPECT_NE(root->arr5[i], nullptr);
        float a = 1.3f + float(i) * 0.4f;
        uint32_t b = uint32_t(i) + 3;
        EXPECT_FLOAT_EQ(root->arr5[i]->a, a);
        EXPECT_EQ(root->arr5[i]->b, b);
    }

    EXPECT_EQ(root->arr6.size(), std::size_t(3));
    EXPECT_EQ(root->arr6[0].size(), std::size_t(2));
    EXPECT_EQ(root->arr6[1].size(), std::size_t(5));
    EXPECT_EQ(root->arr6[2].size(), std::size_t(4));

    EXPECT_EQ(root->arr6[0][0], uint32_t(1));
    EXPECT_EQ(root->arr6[0][1], uint32_t(2));

    EXPECT_EQ(root->arr6[1][0], uint32_t(2));
    EXPECT_EQ(root->arr6[1][1], uint32_t(7));
    EXPECT_EQ(root->arr6[1][2], uint32_t(11));
    EXPECT_EQ(root->arr6[1][3], uint32_t(9));
    EXPECT_EQ(root->arr6[1][4], uint32_t(141));

    EXPECT_EQ(root->arr6[2][0], uint32_t(15));
    EXPECT_EQ(root->arr6[2][1], uint32_t(9));
    EXPECT_EQ(root->arr6[2][2], uint32_t(33));
    EXPECT_EQ(root->arr6[2][3], uint32_t(7));
}

TEST(ZmeyaTestSuite, ArrayTest)
{
    std::vector<char> bytesCopy;
    {
        std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create(1);
        zm::BlobPtr<ArrayTestRoot> root = blobBuilder->allocate<ArrayTestRoot>();

        // assign from std::vector
        std::vector<Payload> vec = {{1.3f, 13}, {2.7f, 27}};
        blobBuilder->copyTo(root->arr1, vec);

        // assign from std::initializer_list
        blobBuilder->copyTo(root->arr2, {2, 4, 6, 10, 14, 32});

        // assign from std::array
        std::array<float, 4> arr{{67.0f, 82.0f, 11.0f, 54.0f}};
        blobBuilder->copyTo(root->arr3, arr);

        // resize array
        blobBuilder->resizeArray(root->arr4, 4);
        // assign array elements (sub-arrays)
        
        blobBuilder->copyTo(blobBuilder->getArrayElement(root->arr4, 0), {1.2f, 2.3f});
        blobBuilder->copyTo(blobBuilder->getArrayElement(root->arr4, 1), {7.1f, 8.8f, 3.2f});
        blobBuilder->copyTo(blobBuilder->getArrayElement(root->arr4, 2), {16.0f, 12.0f, 99.5f, -143.0f});
        blobBuilder->copyTo(blobBuilder->getArrayElement(root->arr4, 3), {-1.0f});

        // resize array
        blobBuilder->resizeArray(root->arr5, 793);
        for (size_t i = 0; i < root->arr5.size(); i++)
        {
            zm::BlobPtr<Payload> payload = blobBuilder->allocate<Payload>(1.3f + float(i) * 0.4f, uint32_t(i) + 3);
            *blobBuilder->getArrayElement(root->arr5, i) = payload;
        }

        std::vector<std::vector<uint32_t>> vec2 = {{1, 2}, {2, 7, 11, 9, 141}, {15, 9, 33, 7}};
        blobBuilder->copyTo(root->arr6, vec2);

        validate(root.get());

        zm::Span<char> bytes = blobBuilder->finalize();
        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const ArrayTestRoot* rootCopy = (const ArrayTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
