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
    std::vector<char> bytesCopy;
    {
        std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create();

        zm::BlobPtr<PointerTestRoot> root = blobBuilder->allocate<PointerTestRoot>();
        zm::BlobPtr<PointerTestNode> nodeLeft = blobBuilder->allocate<PointerTestNode>();
        zm::BlobPtr<PointerTestNode> nodeRight = blobBuilder->allocate<PointerTestNode>();

        root->left = nodeLeft;
        root->right = nodeRight;

        nodeLeft->payload = -13;
        nodeLeft->other = nodeRight;

        nodeRight->payload = 13;
        nodeRight->other = nodeLeft;

        validate(root.get());

        zm::Span<char> bytes = blobBuilder->finalize(16);
        // check (optional) blob size alignment
        EXPECT_TRUE((bytes.size % 16) == 0);

        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const PointerTestRoot* rootCopy = (const PointerTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
