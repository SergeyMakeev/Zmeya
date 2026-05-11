#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct PointerTestNode
{
    int32_t payload;
    zm::Pointer<PointerTestNode> other;
};

struct PointerTestRoot
{
    zm::Pointer<PointerTestNode> left;
    zm::Pointer<PointerTestNode> right;
};

static void validate(const PointerTestRoot* root)
{
    EXPECT_NE(root->left, nullptr);
    EXPECT_NE(root->right, nullptr);
    EXPECT_NE(root->left->other, nullptr);
    EXPECT_NE(root->right->other, nullptr);

    EXPECT_EQ(root->left->payload, -13);
    EXPECT_EQ(root->right->payload, 13);

    EXPECT_EQ(root->left->other->payload, 13);
    EXPECT_EQ(root->right->other->payload, -13);

    EXPECT_EQ(root->right->other, root->left);
    EXPECT_EQ(root->left->other, root->right);
}

TEST(ZmeyaTestSuite, PointerTest)
{
    std::vector<char> bytesCopy = zm::build<PointerTestRoot>(
        [](zm::BuildSession<PointerTestRoot>& session)
        {
            PointerTestRoot* root = session.root();

            PointerTestNode* nodeLeft = session.allocate<PointerTestNode>();
            PointerTestNode* nodeRight = session.allocate<PointerTestNode>();

            root->left = nodeLeft;
            root->right = nodeRight;

            nodeLeft->payload = -13;
            nodeLeft->other = nodeRight;

            nodeRight->payload = 13;
            nodeRight->other = nodeLeft;

            validate(root);
        },
        2048,
        16);

    EXPECT_TRUE((bytesCopy.size() % 16) == 0);

    const PointerTestRoot* rootCopy = (const PointerTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
