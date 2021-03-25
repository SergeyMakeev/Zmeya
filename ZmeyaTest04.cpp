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
        // immediatly reserve 4Mb
        std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create(4 * 1024 * 1024);
        zm::BlobPtr<ListTestRoot> root = blobBuilder->allocate<ListTestRoot>();

#ifdef _DEBUG
        uint32_t numNodes = 3000;
#else
        uint32_t numNodes = 1000000;
#endif

        root->numNodes = numNodes;
        zm::BlobPtr<ListTestNode> prevNode;
        for (uint32_t i = 0; i < numNodes; i++)
        {
            zm::BlobPtr<ListTestNode> node = blobBuilder->allocate<ListTestNode>();
            node->payload = 13 + i;
            node->prev = prevNode;
            if (prevNode)
            {
                prevNode->next = node;
            }
            else
            {
                EXPECT_TRUE(root->root == nullptr);
                root->root = node;
            }
            prevNode = node;
        }

        validate(root.get());

        zm::Span<char> bytes = blobBuilder->finalize();

        bytesCopy = utils::copyBytes(bytes);
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    const ListTestRoot* rootCopy = (const ListTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
