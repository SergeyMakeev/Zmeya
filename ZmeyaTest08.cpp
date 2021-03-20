#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"


struct IteratorsTestRoot
{
    Zmeya::Array<int> arr;
    Zmeya::HashSet<int> set;
    Zmeya::HashMap<int, int> map;
};

static void validate(const IteratorsTestRoot* root)
{
    std::vector<int> temp;

    EXPECT_EQ(root->arr.size(), 11);
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

    EXPECT_EQ(root->set.size(), 6);
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

    EXPECT_EQ(root->map.size(), 3);
    temp.clear();
    temp.resize(root->map.size() * 2, 0);
    for (const Zmeya::Pair<const int, int>& val : root->map)
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
    std::shared_ptr<Zmeya::BlobBuilder> blobBuilder = Zmeya::BlobBuilder::create();
    Zmeya::BlobPtr<IteratorsTestRoot> root = blobBuilder->allocate<IteratorsTestRoot>();

    blobBuilder->copyTo(root->arr, {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0});
    blobBuilder->copyTo(root->set, {0, 1, 4, 3, 5, 2});
    blobBuilder->copyTo(root->map, {{0, 1}, {3, 2}, {4, 5}});

    validate(root.get());

    Zmeya::Span<char> bytes = blobBuilder->finalize();
    std::vector<char> bytesCopy = utils::copyBytes(bytes);
    
    const IteratorsTestRoot* rootCopy = (const IteratorsTestRoot*)(bytesCopy.data());

    validate(rootCopy);
}
