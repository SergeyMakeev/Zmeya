#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct IteratorsTestRoot
{
    zm::Array<int> arr;
    zm::HashSet<int> set;
    zm::HashMap<int, int> map;
};

static void validate(const IteratorsTestRoot* root)
{
    std::vector<int> temp;

    EXPECT_EQ(root->arr.size(), std::size_t(11));
    temp.clear();
    temp.resize(root->arr.size(), 0);
    for (const int& val : root->arr)
    {
        temp[val] += 1;
    }
    for (size_t i = 0; i < temp.size(); i++)
    {
        EXPECT_EQ(temp[i], 1);
    }

    EXPECT_EQ(root->set.size(), std::size_t(6));
    temp.clear();
    temp.resize(root->set.size(), 0);
    for (const int& val : root->set)
    {
        temp[val] += 1;
    }
    for (size_t i = 0; i < temp.size(); i++)
    {
        EXPECT_EQ(temp[i], 1);
    }

    EXPECT_EQ(root->map.size(), std::size_t(3));
    temp.clear();
    temp.resize(root->map.size() * 2, 0);
    for (const zm::Pair<const int, int>& val : root->map)
    {
        temp[val.first] += 1;
        temp[val.second] += 1;
    }
    for (size_t i = 0; i < temp.size(); i++)
    {
        EXPECT_EQ(temp[i], 1);
    }
}

TEST(ZmeyaTestSuite, IteratorsTest)
{
    std::vector<char> bytesCopy;
    {
        std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create();
        zm::BlobPtr<IteratorsTestRoot> root = blobBuilder->allocate<IteratorsTestRoot>();

        blobBuilder->copyTo(root->arr, {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0});
        blobBuilder->copyTo(root->set, {0, 1, 4, 3, 5, 2});
        blobBuilder->copyTo(root->map, {{0, 1}, {3, 2}, {4, 5}});

        validate(root.get());

        zm::Span<char> bytes = blobBuilder->finalize();
        
        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const IteratorsTestRoot* rootCopy = (const IteratorsTestRoot*)(bytesCopy.data());

    validate(rootCopy);
}
