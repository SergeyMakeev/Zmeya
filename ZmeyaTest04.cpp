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
#ifdef _DEBUG
    constexpr size_t kReserve = 4 * 1024 * 1024;
    uint32_t numNodes = 3000;
#else
    constexpr size_t kReserve = 128 * 1024 * 1024;
    uint32_t numNodes = 1000000;
#endif

    std::vector<char> bytesCopy = zm::build<ListTestRoot>(
        [numNodes](zm::BuildSession<ListTestRoot>& session)
        {
            ListTestRoot* root = session.root();

            root->numNodes = numNodes;
            ListTestNode* prevNode = nullptr;
            for (uint32_t i = 0; i < numNodes; i++)
            {
                ListTestNode* node = session.allocate<ListTestNode>();
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

            validate(root);
        },
        kReserve);

    const ListTestRoot* rootCopy = (const ListTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
