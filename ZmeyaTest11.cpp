#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct ReferToTestNode
{
    zm::String str;
    zm::Array<int> arr;
    zm::HashSet<int> hashSet;
    zm::HashMap<zm::String, float> hashMap;
};

struct ReferToTestRoot
{
    zm::String str;
    zm::Array<int32_t> arr;
    zm::HashSet<int32_t> hashSet;
    zm::HashMap<zm::String, float> hashMap;

    zm::Array<ReferToTestNode> nodes;
};

template <typename T> static void validateNode(const T* node)
{
    EXPECT_STREQ(node->str.c_str(), "This is supposed to be a long enough string. I think it is long enough now.");
    EXPECT_EQ(node->arr.size(), std::size_t(10));
    EXPECT_EQ(node->arr[0], 1);
    EXPECT_EQ(node->arr[1], 2);
    EXPECT_EQ(node->arr[2], 5);
    EXPECT_EQ(node->arr[3], 8);
    EXPECT_EQ(node->arr[4], 13);
    EXPECT_EQ(node->arr[5], 99);
    EXPECT_EQ(node->arr[6], 7);
    EXPECT_EQ(node->arr[7], 160);
    EXPECT_EQ(node->arr[8], 293);
    EXPECT_EQ(node->arr[9], 890);

    EXPECT_EQ(node->hashSet.size(), std::size_t(6));
    EXPECT_TRUE(node->hashSet.contains(1));
    EXPECT_TRUE(node->hashSet.contains(5));
    EXPECT_TRUE(node->hashSet.contains(15));
    EXPECT_TRUE(node->hashSet.contains(23));
    EXPECT_TRUE(node->hashSet.contains(38));
    EXPECT_TRUE(node->hashSet.contains(31));
    EXPECT_FALSE(node->hashSet.contains(32));

    EXPECT_EQ(node->hashMap.size(), std::size_t(4));
    EXPECT_FLOAT_EQ(node->hashMap.find("one", -1.0f), 1.0f);
    EXPECT_FLOAT_EQ(node->hashMap.find("two", -1.0f), 2.0f);
    EXPECT_FLOAT_EQ(node->hashMap.find("three", -1.0f), 3.0f);
    EXPECT_FLOAT_EQ(node->hashMap.find("four", -1.0f), 4.0f);
}

static void validate(const ReferToTestRoot* root)
{
    validateNode(root);
    EXPECT_EQ(root->nodes.size(), std::size_t(10000));
    for (size_t i = 0; i < root->nodes.size(); i++)
    {
        validateNode(&root->nodes[i]);
    }
}

TEST(ZmeyaTestSuite, ReferToTest)
{
    std::vector<char> bytesCopy;
    {
        // create blob
        std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create(1);

        // allocate structure
        zm::BlobPtr<ReferToTestRoot> root = blobBuilder->allocate<ReferToTestRoot>();
        blobBuilder->copyTo(root->str, "This is supposed to be a long enough string. I think it is long enough now.");
        blobBuilder->copyTo(root->arr, {1, 2, 5, 8, 13, 99, 7, 160, 293, 890});
        blobBuilder->copyTo(root->hashSet, {1, 5, 15, 23, 38, 31});
        blobBuilder->copyTo(root->hashMap, {{"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}, {"four", 4.0f}});

        float f1 = root->hashMap.find("one", -1.0f);
        EXPECT_FLOAT_EQ(f1, 1.0f);

        float f2 = root->hashMap.find("two", -1.0f);
        EXPECT_FLOAT_EQ(f2, 2.0f);

        float f3 = root->hashMap.find("three", -1.0f);
        EXPECT_FLOAT_EQ(f3, 3.0f);

        float f4 = root->hashMap.find("four", -1.0f);
        EXPECT_FLOAT_EQ(f4, 4.0f);


        size_t nodesCount = 10000;
        blobBuilder->resizeArray(root->nodes, nodesCount);
        for (size_t i = 0; i < nodesCount; i++)
        {
            zm::BlobPtr<ReferToTestNode> node = blobBuilder->getArrayElement(root->nodes, i);
            blobBuilder->referTo(node->str, root->str);
            blobBuilder->referTo(node->arr, root->arr);
            blobBuilder->referTo(node->hashSet, root->hashSet);
            blobBuilder->referTo(node->hashMap, root->hashMap);
        }

        validate(root.get());

        zm::Span<char> bytes = blobBuilder->finalize();
        EXPECT_LE(bytes.size, std::size_t(450000));
        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const ReferToTestRoot* rootCopy = (const ReferToTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
