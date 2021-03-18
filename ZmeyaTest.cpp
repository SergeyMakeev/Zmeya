#include "Zmeya.h"
#include "gtest/gtest.h"

struct SimplePOD
{
    float a;
    int b;
    bool c;
};

static std::vector<char> copyBytes(Zmeya::Span<char> from)
{
    std::vector<char> res;
    res.resize(from.size);
    for (size_t i = 0; i < from.size; i++)
    {
        res[i] = from.data[i];
    }
    return res;
}

TEST(ZmeyaTestSuit, SimpleTest)
{
    // create blob
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();

    // allocate structure
    Zmeya::BlobPtr<SimplePOD> blobStruct = blob->emplace_back<SimplePOD>();

    // change fields
    blobStruct->a = 13.0f;
    blobStruct->b = 1979;
    blobStruct->c = true;

    // finalize blob & copy resulting bytes
    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);

    // test results
    const SimplePOD* obj = (const SimplePOD*)(bytesCopy.data());
    EXPECT_FLOAT_EQ(obj->a, 13.0f);
    EXPECT_EQ(obj->b, 1979);
    EXPECT_EQ(obj->c, true);
}

struct Node
{
    int payload;
    Zmeya::Pointer<Node> other;
};

TEST(ZmeyaTestSuit, PointerTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<Node> node1 = blob->emplace_back<Node>();
    Zmeya::BlobPtr<Node> node2 = blob->emplace_back<Node>();

    node1->payload = 13;
    node1->other = node2;

    node2->payload = 6;
    node2->other = node1;

    Zmeya::Span<char> bytes = blob->finalize(16);
    // check optional blob size alignment
    EXPECT_TRUE((bytes.size % 16) == 0);

    std::vector<char> bytesCopy = copyBytes(bytes);

    // test results
    const Node* obj1 = (const Node*)(bytesCopy.data());
    EXPECT_EQ(obj1->payload, 13);
    const Node* obj2 = obj1->other;
    EXPECT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->payload, 6);
}

struct ArrData
{
    Zmeya::Array<SimplePOD> data1;
    Zmeya::Array<int> data2;
    Zmeya::Array<int> data3;
    Zmeya::Array<int> data4;
    Zmeya::Array<Zmeya::Array<float>> data5;
};

TEST(ZmeyaTestSuit, ArrayTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<ArrData> data = blob->emplace_back<ArrData>();

    std::vector<SimplePOD> vecPods = {{10.5f, 77, false}, {33.5f, 27, true}};
    blob->assign(data->data1, vecPods);
    EXPECT_EQ(data->data1.size(), 2);

    EXPECT_FLOAT_EQ(data->data1[0].a, 10.5f);
    EXPECT_EQ(data->data1[0].b, 77);
    EXPECT_EQ(data->data1[0].c, false);

    EXPECT_FLOAT_EQ(data->data1[1].a, 33.5f);
    EXPECT_EQ(data->data1[1].b, 27);
    EXPECT_EQ(data->data1[1].c, true);

    // assign from std::initializer_list
    blob->assign(data->data2, {2, 4, 6, 10, 14, 32});
    EXPECT_EQ(data->data2.size(), 6);

    EXPECT_EQ(data->data2[0], 2);
    EXPECT_EQ(data->data2[1], 4);
    EXPECT_EQ(data->data2[2], 6);
    EXPECT_EQ(data->data2[3], 10);
    EXPECT_EQ(data->data2[4], 14);
    EXPECT_EQ(data->data2[5], 32);

    // assign from std::vector
    std::vector<int> vecTest{7, 13, 99};
    blob->assign(data->data3, vecTest);
    EXPECT_EQ(data->data3.size(), 3);

    EXPECT_EQ(data->data3[0], 7);
    EXPECT_EQ(data->data3[1], 13);
    EXPECT_EQ(data->data3[2], 99);

    // assign from std::array
    std::array<int, 4> arrTest{67, 82, 11, 54};
    blob->assign(data->data4, arrTest);
    EXPECT_EQ(data->data4.size(), 4);

    EXPECT_EQ(data->data4[0], 67);
    EXPECT_EQ(data->data4[1], 82);
    EXPECT_EQ(data->data4[2], 11);
    EXPECT_EQ(data->data4[3], 54);

    // resize array
    blob->resizeArray(data->data5, 4);
    EXPECT_EQ(data->data5.size(), 4);

    // assign to sub-arrays
    blob->assign(data->data5[0], {1.2f, 2.3f});
    EXPECT_EQ(data->data5[0].size(), 2);

    EXPECT_FLOAT_EQ(data->data5[0][0], 1.2f);
    EXPECT_FLOAT_EQ(data->data5[0][1], 2.3f);

    blob->assign(data->data5[1], {7.1f, 8.8f, 3.2f});
    EXPECT_EQ(data->data5[1].size(), 3);

    EXPECT_FLOAT_EQ(data->data5[1][0], 7.1f);
    EXPECT_FLOAT_EQ(data->data5[1][1], 8.8f);
    EXPECT_FLOAT_EQ(data->data5[1][2], 3.2f);

    blob->assign(data->data5[2], {16.0f, 12.0f, 99.5f, -143.0f});
    EXPECT_EQ(data->data5[2].size(), 4);

    EXPECT_FLOAT_EQ(data->data5[2][0], 16.0f);
    EXPECT_FLOAT_EQ(data->data5[2][1], 12.0f);
    EXPECT_FLOAT_EQ(data->data5[2][2], 99.5f);
    EXPECT_FLOAT_EQ(data->data5[2][3], -143.0f);

    blob->assign(data->data5[3], {-1.0f});
    EXPECT_EQ(data->data5[3].size(), 1);

    EXPECT_FLOAT_EQ(data->data5[3][0], -1.0f);

    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);

    // test results
    const ArrData* obj = (const ArrData*)(bytesCopy.data());

    // data1
    EXPECT_EQ(data->data1.size(), 2);

    EXPECT_FLOAT_EQ(data->data1[0].a, 10.5f);
    EXPECT_EQ(data->data1[0].b, 77);
    EXPECT_EQ(data->data1[0].c, false);

    EXPECT_FLOAT_EQ(data->data1[1].a, 33.5f);
    EXPECT_EQ(data->data1[1].b, 27);
    EXPECT_EQ(data->data1[1].c, true);

    // data 2
    EXPECT_EQ(data->data2.size(), 6);

    EXPECT_EQ(data->data2[0], 2);
    EXPECT_EQ(data->data2[1], 4);
    EXPECT_EQ(data->data2[2], 6);
    EXPECT_EQ(data->data2[3], 10);
    EXPECT_EQ(data->data2[4], 14);
    EXPECT_EQ(data->data2[5], 32);

    // data 3
    EXPECT_EQ(data->data3.size(), 3);

    EXPECT_EQ(data->data3[0], 7);
    EXPECT_EQ(data->data3[1], 13);
    EXPECT_EQ(data->data3[2], 99);

    // data 4
    EXPECT_EQ(data->data4.size(), 4);

    EXPECT_EQ(data->data4[0], 67);
    EXPECT_EQ(data->data4[1], 82);
    EXPECT_EQ(data->data4[2], 11);
    EXPECT_EQ(data->data4[3], 54);

    // data 5
    EXPECT_EQ(data->data5.size(), 4);

    EXPECT_EQ(data->data5[0].size(), 2);
    EXPECT_FLOAT_EQ(data->data5[0][0], 1.2f);
    EXPECT_FLOAT_EQ(data->data5[0][1], 2.3f);

    EXPECT_EQ(data->data5[1].size(), 3);
    EXPECT_FLOAT_EQ(data->data5[1][0], 7.1f);
    EXPECT_FLOAT_EQ(data->data5[1][1], 8.8f);
    EXPECT_FLOAT_EQ(data->data5[1][2], 3.2f);

    EXPECT_EQ(data->data5[2].size(), 4);
    EXPECT_FLOAT_EQ(data->data5[2][0], 16.0f);
    EXPECT_FLOAT_EQ(data->data5[2][1], 12.0f);
    EXPECT_FLOAT_EQ(data->data5[2][2], 99.5f);
    EXPECT_FLOAT_EQ(data->data5[2][3], -143.0f);

    EXPECT_EQ(data->data5[3].size(), 1);
    EXPECT_FLOAT_EQ(data->data5[3][0], -1.0f);
}

struct ListNode
{
    int value;
    Zmeya::Pointer<ListNode> prev;
    Zmeya::Pointer<ListNode> next;
};

struct ListRoot
{
    Zmeya::Pointer<ListNode> root;
};

static void checkList(const ListRoot* root, size_t numNodes)
{
    size_t count = 0;
    const ListNode* currentNode = root->root;
    for (;;)
    {
        if (currentNode == nullptr)
        {
            break;
        }

        EXPECT_EQ(currentNode->value, 13 + count);

        if (count > 0)
        {
            EXPECT_TRUE(currentNode->prev != nullptr);
        }

        if (currentNode->prev)
        {
            EXPECT_EQ(currentNode->prev->value, 13 + count - 1);
        }

        if (currentNode->next)
        {
            EXPECT_EQ(currentNode->next->value, 13 + count + 1);
        }

        count++;
        currentNode = currentNode->next;
    }

    EXPECT_EQ(count, numNodes);
}

TEST(ZmeyaTestSuit, BigListTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create(4 * 1024 * 1024);
    Zmeya::BlobPtr<ListRoot> root = blob->emplace_back<ListRoot>();

#ifdef _DEBUG
    size_t numNodes = 1000;
#else
    size_t numNodes = 1000000;
#endif

    Zmeya::BlobPtr<ListNode> prevNode;
    for (int i = 0; i < numNodes; i++)
    {
        Zmeya::BlobPtr<ListNode> node = blob->emplace_back<ListNode>();
        node->value = 13 + i;
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

    checkList(root.get(), numNodes);

    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);

    const ListRoot* obj = (const ListRoot*)(bytesCopy.data());
    checkList(obj, numNodes);
}

struct StringNode
{
    Zmeya::String str1;
    Zmeya::String str2;
    Zmeya::String str3;
    Zmeya::String str4;
};

TEST(ZmeyaTestSuit, StringTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<StringNode> root = blob->emplace_back<StringNode>();

    blob->assign(root->str1, "Hello world #1");
    EXPECT_STREQ(root->str1.c_str(), "Hello world #1");

    std::string testStr("Hello world #2");
    blob->assign(root->str2, testStr);
    EXPECT_STREQ(root->str2.c_str(), "Hello world #2");

    blob->assign(root->str3, "Hello world #3", 7);
    EXPECT_STREQ(root->str3.c_str(), "Hello w");

    blob->assign(root->str4, "Hello world #1");
    EXPECT_STREQ(root->str4.c_str(), "Hello world #1");

    EXPECT_TRUE(root->str1 == root->str1);
    EXPECT_TRUE(root->str1 == root->str4);
    EXPECT_FALSE(root->str1 == root->str2);
    EXPECT_FALSE(root->str1 == root->str3);

    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);

    const StringNode* obj = (const StringNode*)(bytesCopy.data());

    EXPECT_STREQ(obj->str1.c_str(), "Hello world #1");
    EXPECT_STREQ(obj->str2.c_str(), "Hello world #2");
    EXPECT_STREQ(obj->str3.c_str(), "Hello w");

    EXPECT_TRUE(obj->str1 == obj->str1);
    EXPECT_TRUE(obj->str1 == obj->str4);
    EXPECT_FALSE(obj->str1 == obj->str2);
    EXPECT_FALSE(obj->str1 == obj->str3);
}

struct HSetNode
{
    Zmeya::HashSet<int> uniqueNums;
    Zmeya::HashSet<Zmeya::String> uniqueStrings;
};

TEST(ZmeyaTestSuit, HashSetTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<HSetNode> root = blob->emplace_back<HSetNode>();

    std::unordered_set<int> testSet1 = {5, 7, 3, 11};
    blob->assign(root->uniqueNums, testSet1);
    EXPECT_TRUE(root->uniqueNums.contains(5));
    EXPECT_TRUE(root->uniqueNums.contains(7));
    EXPECT_TRUE(root->uniqueNums.contains(3));
    EXPECT_TRUE(root->uniqueNums.contains(11));
    EXPECT_FALSE(root->uniqueNums.contains(1));
    EXPECT_FALSE(root->uniqueNums.contains(6));
    EXPECT_FALSE(root->uniqueNums.contains(15));
    EXPECT_FALSE(root->uniqueNums.contains(88));

    std::unordered_set<std::string> testSet2 = {"one", "two", "three", "four"};
    blob->assign(root->uniqueStrings, testSet2);
    // TODO ^^^^

    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);

    const HSetNode* obj = (const HSetNode*)(bytesCopy.data());
    EXPECT_TRUE(obj->uniqueNums.contains(5));
    EXPECT_TRUE(obj->uniqueNums.contains(7));
    EXPECT_TRUE(obj->uniqueNums.contains(3));
    EXPECT_TRUE(obj->uniqueNums.contains(11));
    EXPECT_FALSE(obj->uniqueNums.contains(1));
    EXPECT_FALSE(obj->uniqueNums.contains(6));
    EXPECT_FALSE(obj->uniqueNums.contains(15));
    EXPECT_FALSE(obj->uniqueNums.contains(88));
}

struct HMapNode
{
    Zmeya::HashMap<int, float> hashMap1;
    Zmeya::HashMap<Zmeya::String, float> hashMap2;
};

TEST(ZmeyaTestSuit, HashMapTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<HMapNode> root = blob->emplace_back<HMapNode>();

    std::unordered_map<int, float> testMap = {{3, 7.0f}, {4, 17.0f}, {9, 79.0f}, {11, 13.0f}};
    blob->assign(root->hashMap1, testMap);
    EXPECT_FLOAT_EQ(root->hashMap1.find(3, -1.0f), 7.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(4, -1.0f), 17.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(9, -1.0f), 79.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(11, -1.0f), 13.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(12, -1.0f), -1.0f);
    EXPECT_EQ(root->hashMap1.find(99), nullptr);
    EXPECT_NE(root->hashMap1.find(3), nullptr);
    EXPECT_EQ(root->hashMap1.contains(99), false);
    EXPECT_EQ(root->hashMap1.contains(3), true);

    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);
    const HMapNode* obj = (const HMapNode*)(bytesCopy.data());
    EXPECT_FLOAT_EQ(obj->hashMap1.find(3, -1.0f), 7.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(4, -1.0f), 17.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(9, -1.0f), 79.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(11, -1.0f), 13.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(12, -1.0f), -1.0f);
    EXPECT_EQ(obj->hashMap1.find(99), nullptr);
    EXPECT_NE(obj->hashMap1.find(3), nullptr);
    EXPECT_EQ(obj->hashMap1.contains(99), false);
    EXPECT_EQ(obj->hashMap1.contains(3), true);
}
