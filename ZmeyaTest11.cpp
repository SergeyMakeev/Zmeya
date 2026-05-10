#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*

Originally exercised BlobBuilder referTo() for shared sub-blobs (small on-disk size).
The new builder deep-copies those payloads into each node; semantics on read stay the same.

*/

struct ReferToTestNode
{
    zm::String str;
    zm::Array<int> arr;
    zm::HashSet<int> hashSet;
    zm::HashMap<zm::String, float> hashMap;

    template <typename Init>
    ReferToTestNode& operator=(const Init& o)
    {
        str = o.str;
        arr = o.arr;
        hashSet = o.hashSet;
        hashMap = o.hashMap;
        return *this;
    }
};

struct ReferToTestRoot
{
    zm::String str;
    zm::Array<int32_t> arr;
    zm::HashSet<int32_t> hashSet;
    zm::HashMap<zm::String, float> hashMap;

    zm::Array<ReferToTestNode> nodes;
};

struct ReferToNodeInit
{
    std::string str;
    std::vector<int> arr;
    std::unordered_set<int> hashSet;
    std::unordered_map<std::string, float> hashMap;
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
        std::unique_ptr<zm::Builder<ReferToTestRoot>> builder = zm::Builder<ReferToTestRoot>::create(128 * 1024 * 1024);
        zm::ScopedBuilder scope(builder.get());

        ReferToTestRoot* root = builder->getRoot();

        root->str = std::string("This is supposed to be a long enough string. I think it is long enough now.");
        root->arr = std::vector<int32_t>{1, 2, 5, 8, 13, 99, 7, 160, 293, 890};
        root->hashSet = std::unordered_set<int32_t>{1, 5, 15, 23, 38, 31};
        root->hashMap =
            std::unordered_map<std::string, float>{{"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}, {"four", 4.0f}};

        EXPECT_FLOAT_EQ(root->hashMap.find("one", -1.0f), 1.0f);
        EXPECT_FLOAT_EQ(root->hashMap.find("two", -1.0f), 2.0f);
        EXPECT_FLOAT_EQ(root->hashMap.find("three", -1.0f), 3.0f);
        EXPECT_FLOAT_EQ(root->hashMap.find("four", -1.0f), 4.0f);

        ReferToNodeInit proto;
        proto.str = "This is supposed to be a long enough string. I think it is long enough now.";
        proto.arr = {1, 2, 5, 8, 13, 99, 7, 160, 293, 890};
        proto.hashSet = {1, 5, 15, 23, 38, 31};
        proto.hashMap = {{"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}, {"four", 4.0f}};

        std::vector<ReferToNodeInit> nodeInits;
        nodeInits.resize(10000, proto);

        zm::assign(root->nodes, nodeInits);

        validate(root);

        zm::Span<char> bytes = builder->finalize();
        (void)bytes;
        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const ReferToTestRoot* rootCopy = (const ReferToTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
