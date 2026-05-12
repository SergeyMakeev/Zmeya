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
    uint32_t numNodes = 3000;
#else
    uint32_t numNodes = 1000000;
#endif

    zm::BlobBuffer bytesCopy = zm::write_scope<ListTestRoot>(
        [numNodes](zm::BlobWriter<ListTestRoot>& w)
        {
            zm::detail::BuilderBase* bb = w.builder_base();
            ListTestRoot* root = w.root();

            root->numNodes = numNodes;
            zm::goffset_t prev_g{};
            for (uint32_t i = 0; i < numNodes; i++)
            {
                ListTestNode* node = w.allocate<ListTestNode>();
                node->payload = 13 + i;
                node->prev = nullptr;
                if (i > 0)
                {
                    ListTestNode* prevNode = reinterpret_cast<ListTestNode*>(bb->get_ptr_unsafe_to_store(prev_g));
                    node->prev = prevNode;
                    prevNode->next = node;
                }
                else
                {
                    EXPECT_TRUE(w.root()->root == nullptr);
                    w.root()->root = node;
                }
                prev_g = bb->arena_byte_offset_of(node);
            }

            validate(w.root());
        });

    const ListTestRoot* rootCopy = (const ListTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}
