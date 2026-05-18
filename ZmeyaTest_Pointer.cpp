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
    zm::BlobBuffer bytesCopy = zm::write_scope<PointerTestRoot>(
        [](zm::BlobWriter<PointerTestRoot>& w)
        {
            zm::ArenaRef<PointerTestRoot> root = w.root();

            zm::ArenaRef<PointerTestNode> nodeLeft = w.allocate<PointerTestNode>();
            zm::ArenaRef<PointerTestNode> nodeRight = w.allocate<PointerTestNode>();

            root->left = nodeLeft.transient_ptr();
            root->right = nodeRight.transient_ptr();

            nodeLeft->payload = -13;
            nodeLeft->other = nodeRight.transient_ptr();

            nodeRight->payload = 13;
            nodeRight->other = nodeLeft.transient_ptr();

            validate(root.transient_ptr());
        },
        16);

    EXPECT_TRUE((bytesCopy.size() % 16) == 0);

    const PointerTestRoot* rootCopy = (const PointerTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
