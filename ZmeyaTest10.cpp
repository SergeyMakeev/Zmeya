#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#if _WIN32
#include <Windows.h>
#endif

/*

This test is a nightmare for any serialization system (not for Zmeya)
a ton of different objects, everything is linked by pointers,
plus inheritance and different hash containers.

*/

enum class NodeType : uint32_t
{
    NodeType1 = 1,
    NodeType2 = 2,
    Leaf = 3,
};

struct MMapTestNode
{
    zm::String name;
    NodeType nodeType;
    zm::Array<zm::Pointer<MMapTestNode>> children;
};

struct MMapTestRoot
{
    uint32_t magic;
    zm::String desc;
    zm::HashMap<zm::String, float> hashMap;
    zm::Array<zm::Pointer<MMapTestNode>> roots;
};

struct MMapTestLeafNode : public MMapTestNode
{
    uint32_t payload;
    zm::Pointer<MMapTestNode> parent;
};

struct MMapTestNode1 : public MMapTestNode
{
    zm::String str1;
    uint32_t idx;
    zm::Pointer<MMapTestRoot> root;
};

struct MMapTestNode2 : public MMapTestNode
{
    zm::String str1;
    zm::HashSet<int32_t> hashSet;
};

static void validateChildren(const MMapTestNode* parent, size_t count, size_t startIndex)
{
    EXPECT_EQ(parent->children.size(), count);

    for (size_t i = 0; i < count; i++)
    {
        const MMapTestNode* nodeBase = parent->children[i].get();
        EXPECT_EQ(nodeBase->nodeType, NodeType::Leaf);
        std::string expectedName = "leaf_" + std::to_string(startIndex + i);
        EXPECT_EQ(nodeBase->name, expectedName);
        if (nodeBase->nodeType != NodeType::Leaf)
        {
            continue;
        }

        const MMapTestLeafNode* node = reinterpret_cast<const MMapTestLeafNode*>(nodeBase);
        EXPECT_EQ(node->payload, uint32_t(count + startIndex * 13));
        EXPECT_EQ(node->parent.get(), parent);
    }
}

static void validateNode1(const MMapTestNode* nodeBase, size_t index)
{
    EXPECT_EQ(nodeBase->nodeType, NodeType::NodeType1);
    std::string expectedName = "node_" + std::to_string(index);
    EXPECT_EQ(nodeBase->name, expectedName);
    if (nodeBase->nodeType != NodeType::NodeType1)
    {
        return;
    }

    const MMapTestNode1* node = reinterpret_cast<const MMapTestNode1*>(nodeBase);
    EXPECT_EQ(node->str1, "Zmyea test file. This is supposed to be a long enough string. I think it is long enough now.");
    EXPECT_EQ(node->idx, uint32_t(index));
    size_t numChildrenNodes = 1 + (index % 6);
    validateChildren(node, numChildrenNodes, index);
}
static void validateNode2(const MMapTestNode* nodeBase, size_t index)
{
    EXPECT_EQ(nodeBase->nodeType, NodeType::NodeType2);
    std::string expectedName = "item_" + std::to_string(index);
    EXPECT_EQ(nodeBase->name, expectedName);
    if (nodeBase->nodeType != NodeType::NodeType2)
    {
        return;
    }
    const MMapTestNode2* node = reinterpret_cast<const MMapTestNode2*>(nodeBase);
    EXPECT_EQ(node->str1, "Zmyea test file. This is supposed to be a long enough string. I think it is long enough now.");

    EXPECT_EQ(node->hashSet.size(), std::size_t(3));
    EXPECT_TRUE(node->hashSet.contains(int32_t(index + 1)));
    EXPECT_TRUE(node->hashSet.contains(int32_t(index + 2)));
    EXPECT_TRUE(node->hashSet.contains(int32_t(index + 3)));
    size_t numChildrenNodes = 2;
    validateChildren(node, numChildrenNodes, index);
}

static void validate(const MMapTestRoot* root)
{
    EXPECT_EQ(root->magic, 0x59454D5Au);
    EXPECT_EQ(root->desc, "Zmyea test file. This is supposed to be a long enough string. I think it is long enough now.");

    EXPECT_EQ(root->hashMap.size(), std::size_t(6));
    EXPECT_FLOAT_EQ(root->hashMap.find("one", 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("two", 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("three", 0.0f), 3.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("four", 0.0f), 4.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("five", 0.0f), 5.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("six", 0.0f), 6.0f);

    EXPECT_EQ(root->roots.size(), std::size_t(512));
    for (size_t i = 0; i < root->roots.size(); i++)
    {
        const zm::Pointer<MMapTestNode>& rootNode = root->roots[i];
        if ((i & 1) == 0)
        {
            validateNode1(rootNode.get(), i);
        }
        else
        {
            validateNode2(rootNode.get(), i);
        }
    }
}

void createChildren(zm::BlobBuilder* blobBuilder, const zm::BlobPtr<MMapTestNode>& parent, size_t count, size_t startIndex)
{
    for (size_t i = 0; i < count; i++)
    {
        zm::BlobPtr<MMapTestLeafNode> node = blobBuilder->allocate<MMapTestLeafNode>();
        node->nodeType = NodeType::Leaf;
        blobBuilder->copyTo(node->name, "leaf_" + std::to_string(startIndex + i));
        node->payload = uint32_t(count + startIndex * 13);
        node->parent = parent;
        parent->children[i] = node;
    }
}

zm::BlobPtr<MMapTestNode> allocateNode1(zm::BlobBuilder* blobBuilder, const zm::BlobPtr<MMapTestRoot>& root, size_t index)
{
    zm::BlobPtr<MMapTestNode1> node = blobBuilder->allocate<MMapTestNode1>();
    node->nodeType = NodeType::NodeType1;
    blobBuilder->copyTo(node->name, "node_" + std::to_string(index));
    blobBuilder->referTo(node->str1, root->desc);
    node->idx = uint32_t(index);
    size_t numChildrenNodes = 1 + (index % 6);
    blobBuilder->resizeArray(node->children, numChildrenNodes);
    createChildren(blobBuilder, node, numChildrenNodes, index);
    return node;
}

zm::BlobPtr<MMapTestNode> allocateNode2(zm::BlobBuilder* blobBuilder, const zm::BlobPtr<MMapTestRoot>& root, size_t index)
{
    zm::BlobPtr<MMapTestNode2> node = blobBuilder->allocate<MMapTestNode2>();
    node->nodeType = NodeType::NodeType2;
    blobBuilder->copyTo(node->name, "item_" + std::to_string(index));
    blobBuilder->referTo(node->str1, root->desc);
    blobBuilder->copyTo(node->hashSet, {int32_t(index + 1), int32_t(index + 2), int32_t(index + 3)});
    size_t numChildrenNodes = 2;
    blobBuilder->resizeArray(node->children, numChildrenNodes);
    createChildren(blobBuilder, node, numChildrenNodes, index);
    return node;
}

static void generateTestFile(const char* fileName)
{
    std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create();
    zm::BlobPtr<MMapTestRoot> root = blobBuilder->allocate<MMapTestRoot>();
    root->magic = 0x59454D5A;
    blobBuilder->copyTo(root->desc, "Zmyea test file. This is supposed to be a long enough string. I think it is long enough now.");
    blobBuilder->copyTo(root->hashMap, {{"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}, {"four", 4.0f}, {"five", 5.0f}, {"six", 6.0f}});
    size_t numRoots = 512;
    blobBuilder->resizeArray(root->roots, numRoots);
    for (size_t i = 0; i < 512; i++)
    {
        zm::BlobPtr<MMapTestNode> rootNode;
        if ((i & 1) == 0)
        {
            rootNode = allocateNode1(blobBuilder.get(), root, i);
        }
        else
        {
            rootNode = allocateNode2(blobBuilder.get(), root, i);
        }
        root->roots[i] = rootNode;
    }

    validate(root.get());

    zm::Span<char> bytes = blobBuilder->finalize(32);
    EXPECT_TRUE((bytes.size % 32) == std::size_t(0));

    FILE* file = fopen(fileName, "wb");
    ASSERT_TRUE(file != nullptr);
    fwrite(bytes.data, bytes.size, 1, file);
    fclose(file);
}

TEST(ZmeyaTestSuite, MMapTest)
{
    const char* fileName = "mmaptest.zmy";
    generateTestFile(fileName);

#if _WIN32
    // use memory mapped file view to access the data
    HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT_TRUE(hFile != INVALID_HANDLE_VALUE);

    LARGE_INTEGER fileSizeInBytes;
    BOOL res = GetFileSizeEx(hFile, &fileSizeInBytes);
    ASSERT_TRUE(res);

    HANDLE hMapping = CreateFileMapping(hFile, 0, PAGE_READONLY | SEC_COMMIT, fileSizeInBytes.HighPart, fileSizeInBytes.LowPart, 0);
    ASSERT_TRUE(hMapping != NULL);

    const MMapTestRoot* fileRoot = (const MMapTestRoot*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, size_t(fileSizeInBytes.QuadPart));
    ASSERT_TRUE(fileRoot != nullptr);

    validate(fileRoot);

    UnmapViewOfFile(fileRoot);
    CloseHandle(hMapping);
    CloseHandle(hFile);
#endif
}
