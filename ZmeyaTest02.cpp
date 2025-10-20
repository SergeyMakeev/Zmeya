#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#if 0

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
        std::shared_ptr<zm::Builder> _builder = zm::Builder::create();
        zm::ScopedBuilder scope(_builder.get());
        
        zm::Builder* builder = zm::detail::get_global_builder();
        ZMEYA_ASSERT(builder != nullptr);

        PointerTestRoot* root = builder->allocate_root<PointerTestRoot>();
        PointerTestNode* nodeLeft = builder->allocate<PointerTestNode>();
        PointerTestNode* nodeRight = builder->allocate<PointerTestNode>();

        zm::assign(root->left, nodeLeft);
        zm::assign(root->right, nodeRight);

        nodeLeft->payload = -13;
        zm::assign(nodeLeft->other, nodeRight);

        nodeRight->payload = 13;
        zm::assign(nodeRight->other, nodeLeft);

        validate(root);

        zm::Span<char> bytes = builder->finalize(16);
        // check (optional) blob size alignment
        EXPECT_TRUE((bytes.size % 16) == 0);

        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const PointerTestRoot* rootCopy = (const PointerTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}


#endif