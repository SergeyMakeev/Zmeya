#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

/*

Stress layout: many nodes, pointers, inheritance, hash containers.
Shared payload strings are deep-copied per node (larger blob than pointer-sharing would allow).

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

static const char kLongDesc[] =
    "Zmyea test file. This is supposed to be a long enough string. I think it is long enough now.";

static void validateChildren(const MMapTestNode* parent, size_t count, size_t startIndex)
{
    EXPECT_EQ(parent->children.size(), count);

    for (size_t i = 0; i < count; i++)
    {
        const MMapTestNode* nodeBase = parent->children[i].get();
        EXPECT_EQ(nodeBase->nodeType, NodeType::Leaf);
        std::string expectedName = "leaf_" + std::to_string(startIndex + i);
        EXPECT_EQ(nodeBase->name, expectedName);
        ZMEYA_ASSERT(nodeBase->nodeType == NodeType::Leaf);

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
    ZMEYA_ASSERT(nodeBase->nodeType == NodeType::NodeType1);

    const MMapTestNode1* node = reinterpret_cast<const MMapTestNode1*>(nodeBase);
    EXPECT_EQ(node->str1, kLongDesc);
    EXPECT_EQ(node->idx, uint32_t(index));
    size_t numChildrenNodes = 1 + (index % 6);
    validateChildren(node, numChildrenNodes, index);
}

static void validateNode2(const MMapTestNode* nodeBase, size_t index)
{
    EXPECT_EQ(nodeBase->nodeType, NodeType::NodeType2);
    std::string expectedName = "item_" + std::to_string(index);
    EXPECT_EQ(nodeBase->name, expectedName);
    ZMEYA_ASSERT(nodeBase->nodeType == NodeType::NodeType2);

    const MMapTestNode2* node = reinterpret_cast<const MMapTestNode2*>(nodeBase);
    EXPECT_EQ(node->str1, kLongDesc);

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
    EXPECT_EQ(root->desc, kLongDesc);

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

static void createChildren(zm::BlobWriter<MMapTestRoot>& w, MMapTestNode* parent, size_t count, size_t startIndex)
{
    std::vector<MMapTestNode*> childPtrs;
    childPtrs.reserve(count);
    for (size_t i = 0; i < count; i++)
    {
        MMapTestLeafNode* leaf = w.allocate<MMapTestLeafNode>();
        leaf->nodeType = NodeType::Leaf;
        leaf->name = std::string("leaf_") + std::to_string(startIndex + i);
        leaf->payload = uint32_t(count + startIndex * 13);
        leaf->parent = parent;
        childPtrs.push_back(leaf);
    }
    parent->children = childPtrs;
}

static MMapTestNode* allocateNode1(zm::BlobWriter<MMapTestRoot>& w, MMapTestRoot* root, size_t index)
{
    MMapTestNode1* node = w.allocate<MMapTestNode1>();
    node->nodeType = NodeType::NodeType1;
    node->name = std::string("node_") + std::to_string(index);
    node->str1 = std::string(kLongDesc);
    node->idx = uint32_t(index);
    node->root = root;

    size_t numChildrenNodes = 1 + (index % 6);
    createChildren(w, node, numChildrenNodes, index);
    return node;
}

static MMapTestNode* allocateNode2(zm::BlobWriter<MMapTestRoot>& w, size_t index)
{
    MMapTestNode2* node = w.allocate<MMapTestNode2>();
    node->nodeType = NodeType::NodeType2;
    node->name = std::string("item_") + std::to_string(index);
    node->str1 = std::string(kLongDesc);

    std::unordered_set<int32_t> hs = {int32_t(index + 1), int32_t(index + 2), int32_t(index + 3)};
    node->hashSet = hs;

    size_t numChildrenNodes = 2;
    createChildren(w, node, numChildrenNodes, index);
    return node;
}

static void generateTestFile(const char* fileName)
{
    constexpr size_t kStartArenaBytes = 64u * 1024u * 1024u;
    std::vector<char> bytes = zm::detail::write_blob_with_initial_buffer_bytes<MMapTestRoot>(
        [](zm::BlobWriter<MMapTestRoot>& w)
        {
            MMapTestRoot* root = w.root();
            root->magic = 0x59454D5A;
            root->desc = std::string(kLongDesc);

            std::unordered_map<std::string, float> hm = {
                {"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}, {"four", 4.0f}, {"five", 5.0f}, {"six", 6.0f}};
            root->hashMap = hm;

            constexpr size_t numRoots = 512;
            std::vector<MMapTestNode*> rootNodes;
            rootNodes.reserve(numRoots);
            for (size_t i = 0; i < numRoots; i++)
            {
                if ((i & 1) == 0)
                {
                    rootNodes.push_back(allocateNode1(w, root, i));
                }
                else
                {
                    rootNodes.push_back(allocateNode2(w, i));
                }
            }
            root->roots = rootNodes;

            validate(root);
        },
        kStartArenaBytes,
        32);

    EXPECT_TRUE((bytes.size() % 32) == std::size_t(0));

    FILE* file = fopen(fileName, "wb");
    ASSERT_TRUE(file != nullptr);
    fwrite(bytes.data(), bytes.size(), 1, file);
    fclose(file);
}

TEST(ZmeyaTestSuite, MMapTest)
{
    const char* fileName = "mmaptest.zm";
    generateTestFile(fileName);

#if defined(_WIN32)
    HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT_TRUE(hFile != INVALID_HANDLE_VALUE);

    LARGE_INTEGER fileSizeInBytes;
    BOOL res = GetFileSizeEx(hFile, &fileSizeInBytes);
    ASSERT_TRUE(res);

    HANDLE hMapping =
        CreateFileMapping(hFile, 0, PAGE_READONLY | SEC_COMMIT, fileSizeInBytes.HighPart, fileSizeInBytes.LowPart, 0);
    ASSERT_TRUE(hMapping != NULL);

    const MMapTestRoot* fileRoot =
        (const MMapTestRoot*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, size_t(fileSizeInBytes.QuadPart));
    ASSERT_TRUE(fileRoot != nullptr);

    validate(fileRoot);

    UnmapViewOfFile(fileRoot);
    CloseHandle(hMapping);
    CloseHandle(hFile);
#else
    FILE* file = fopen(fileName, "rb");
    ASSERT_TRUE(file != nullptr);
    fseek(file, 0L, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    std::vector<char> buffer(size_t(fileSize));
    ASSERT_EQ(fread(buffer.data(), size_t(fileSize), 1, file), size_t(1));
    fclose(file);

    const MMapTestRoot* fileRoot = reinterpret_cast<const MMapTestRoot*>(buffer.data());
    validate(fileRoot);
#endif
}
