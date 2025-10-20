#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct ListTestNode
{
    uint32_t payload;
    zm::Pointer<ListTestNode> prev;
    zm::Pointer<ListTestNode> next;
};

struct ListTestRoot
{
    uint32_t numNodes;
    zm::Pointer<ListTestNode> root;
};

static void validate(const ListTestRoot* root)
{
    uint32_t count = 0;
    uint32_t numNodes = root->numNodes;
    const ListTestNode* currentNode = root->root.get();
    for (;;)
    {
        if (currentNode == nullptr)
        {
            break;
        }

        EXPECT_EQ(currentNode->payload, 13 + count);
        if (count > 0)
        {
            EXPECT_TRUE(currentNode->prev != nullptr);
        }
        if (currentNode->prev)
        {
            EXPECT_EQ(currentNode->prev->payload, 13 + count - 1);
        }
        if (currentNode->next)
        {
            EXPECT_EQ(currentNode->next->payload, 13 + count + 1);
        }

        count++;
        currentNode = currentNode->next.get();
    }
    EXPECT_EQ(count, numNodes);
}

TEST(ZmeyaTestSuite, ListTest)
{
    std::vector<char> bytesCopy;
    {
        // create builder with 4MB initial size
        std::shared_ptr<zm::Builder> _builder = zm::Builder::create(4 * 1024 * 1024);
        zm::ScopedBuilder scope(_builder.get());
        
        zm::Builder* builder = zm::detail::get_global_builder();
        ZMEYA_ASSERT(builder != nullptr);

        ListTestRoot* root = builder->allocate_root<ListTestRoot>();

#ifdef _DEBUG
        uint32_t numNodes = 3000;
#else
        uint32_t numNodes = 1000000;
#endif

        root->numNodes = numNodes;
        ListTestNode* prevNode = nullptr;
        for (uint32_t i = 0; i < numNodes; i++)
        {
            ListTestNode* node = builder->allocate<ListTestNode>();
            node->payload = 13 + i;
            zm::assign(node->prev, prevNode);
            if (prevNode)
            {
                zm::assign(prevNode->next, node);
            }
            else
            {
                EXPECT_TRUE(root->root == nullptr);
                zm::assign(root->root, node);
            }
            prevNode = node;
        }

        validate(root);

        zm::Span<char> bytes = builder->finalize();

        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const ListTestRoot* rootCopy = (const ListTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
