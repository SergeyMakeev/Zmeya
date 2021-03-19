#include "gtest/gtest.h"

#define ZMEYA_ENABLE_SERIALIZE_SUPPORT
#include "Zmeya.h"

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

TEST(ZmeyaTestSuite1, SimpleTest)
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

TEST(ZmeyaTestSuite1, PointerTest)
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

TEST(ZmeyaTestSuite1, ArrayTest)
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

TEST(ZmeyaTestSuite1, BigListTest)
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
    Zmeya::Array<Zmeya::String> strArray;
    Zmeya::Array<Zmeya::String> strArray2;
    Zmeya::Array<Zmeya::String> strArray3;
};

TEST(ZmeyaTestSuite1, StringTest)
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

    std::vector<std::string> from = {"first", "second", "third", "fourth"};
    blob->assign(root->strArray, from);
    EXPECT_EQ(root->strArray.size(), 4);
    EXPECT_STREQ(root->strArray[0].c_str(), "first");
    EXPECT_STREQ(root->strArray[1].c_str(), "second");
    EXPECT_STREQ(root->strArray[2].c_str(), "third");
    EXPECT_STREQ(root->strArray[3].c_str(), "fourth");

    blob->assign(root->strArray2, {"one", "two", "three"});
    EXPECT_EQ(root->strArray2.size(), 3);
    EXPECT_STREQ(root->strArray2[0].c_str(), "one");
    EXPECT_STREQ(root->strArray2[1].c_str(), "two");
    EXPECT_STREQ(root->strArray2[2].c_str(), "three");

    std::array<const char*, 2> from2 = {"hello", "world"};
    blob->assign(root->strArray3, from2);
    EXPECT_EQ(root->strArray3.size(), 2);
    EXPECT_STREQ(root->strArray3[0].c_str(), "hello");
    EXPECT_STREQ(root->strArray3[1].c_str(), "world");

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

    EXPECT_EQ(obj->strArray.size(), 4);
    EXPECT_STREQ(obj->strArray[0].c_str(), "first");
    EXPECT_STREQ(obj->strArray[1].c_str(), "second");
    EXPECT_STREQ(obj->strArray[2].c_str(), "third");
    EXPECT_STREQ(obj->strArray[3].c_str(), "fourth");

    EXPECT_EQ(obj->strArray2.size(), 3);
    EXPECT_STREQ(obj->strArray2[0].c_str(), "one");
    EXPECT_STREQ(obj->strArray2[1].c_str(), "two");
    EXPECT_STREQ(obj->strArray2[2].c_str(), "three");

    EXPECT_EQ(obj->strArray3.size(), 2);
    EXPECT_STREQ(obj->strArray3[0].c_str(), "hello");
    EXPECT_STREQ(obj->strArray3[1].c_str(), "world");
}

struct HSetNode
{
    Zmeya::HashSet<int> uniqueNums;
    Zmeya::HashSet<int> uniqueNums2;
    Zmeya::HashSet<Zmeya::String> uniqueStrings;
    Zmeya::HashSet<Zmeya::String> uniqueStrings2;
};

TEST(ZmeyaTestSuite1, HashSetTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<HSetNode> root = blob->emplace_back<HSetNode>();

    std::unordered_set<int> testSet1 = {5, 7, 3, 11, 99};
    blob->assign(root->uniqueNums, testSet1);
    EXPECT_EQ(root->uniqueNums.size(), 5);
    EXPECT_TRUE(root->uniqueNums.contains(99));
    EXPECT_TRUE(root->uniqueNums.contains(5));
    EXPECT_TRUE(root->uniqueNums.contains(7));
    EXPECT_TRUE(root->uniqueNums.contains(3));
    EXPECT_TRUE(root->uniqueNums.contains(11));
    EXPECT_FALSE(root->uniqueNums.contains(1));
    EXPECT_FALSE(root->uniqueNums.contains(6));
    EXPECT_FALSE(root->uniqueNums.contains(15));
    EXPECT_FALSE(root->uniqueNums.contains(88));

    blob->assign(root->uniqueNums2, {1, 1, 2, 3, 4, 4, 0, 99, 1, 6, 3});
    EXPECT_EQ(root->uniqueNums2.size(), 7);
    EXPECT_TRUE(root->uniqueNums2.contains(1));
    EXPECT_TRUE(root->uniqueNums2.contains(2));
    EXPECT_TRUE(root->uniqueNums2.contains(3));
    EXPECT_TRUE(root->uniqueNums2.contains(4));
    EXPECT_TRUE(root->uniqueNums2.contains(0));
    EXPECT_TRUE(root->uniqueNums2.contains(99));
    EXPECT_TRUE(root->uniqueNums2.contains(6));
    EXPECT_FALSE(root->uniqueNums2.contains(7));
    EXPECT_FALSE(root->uniqueNums2.contains(-2));
    EXPECT_FALSE(root->uniqueNums2.contains(11));

    std::unordered_set<std::string> testSet2 = {"one", "two", "three", "four"};
    blob->assign(root->uniqueStrings, testSet2);
    EXPECT_EQ(root->uniqueStrings.size(), 4);
    EXPECT_FALSE(root->uniqueStrings.contains("zero"));
    EXPECT_TRUE(root->uniqueStrings.contains("one"));
    EXPECT_TRUE(root->uniqueStrings.contains("two"));
    EXPECT_TRUE(root->uniqueStrings.contains("three"));
    EXPECT_TRUE(root->uniqueStrings.contains("four"));
    EXPECT_FALSE(root->uniqueStrings.contains("five"));

    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);

    const HSetNode* obj = (const HSetNode*)(bytesCopy.data());

    EXPECT_EQ(obj->uniqueNums.size(), 5);
    EXPECT_TRUE(obj->uniqueNums.contains(99));
    EXPECT_TRUE(obj->uniqueNums.contains(5));
    EXPECT_TRUE(obj->uniqueNums.contains(7));
    EXPECT_TRUE(obj->uniqueNums.contains(3));
    EXPECT_TRUE(obj->uniqueNums.contains(11));
    EXPECT_FALSE(obj->uniqueNums.contains(1));
    EXPECT_FALSE(obj->uniqueNums.contains(6));
    EXPECT_FALSE(obj->uniqueNums.contains(15));
    EXPECT_FALSE(obj->uniqueNums.contains(88));

    EXPECT_EQ(root->uniqueNums2.size(), 7);
    EXPECT_TRUE(root->uniqueNums2.contains(1));
    EXPECT_TRUE(root->uniqueNums2.contains(2));
    EXPECT_TRUE(root->uniqueNums2.contains(3));
    EXPECT_TRUE(root->uniqueNums2.contains(4));
    EXPECT_TRUE(root->uniqueNums2.contains(0));
    EXPECT_TRUE(root->uniqueNums2.contains(99));
    EXPECT_TRUE(root->uniqueNums2.contains(6));
    EXPECT_FALSE(root->uniqueNums2.contains(7));
    EXPECT_FALSE(root->uniqueNums2.contains(-2));
    EXPECT_FALSE(root->uniqueNums2.contains(11));

    EXPECT_EQ(obj->uniqueStrings.size(), 4);
    EXPECT_FALSE(obj->uniqueStrings.contains("zero"));
    EXPECT_TRUE(obj->uniqueStrings.contains("one"));
    EXPECT_TRUE(obj->uniqueStrings.contains("two"));
    EXPECT_TRUE(obj->uniqueStrings.contains("three"));
    EXPECT_TRUE(obj->uniqueStrings.contains("four"));
    EXPECT_FALSE(obj->uniqueStrings.contains("five"));
}

struct HMapNode
{
    Zmeya::HashMap<int, float> hashMap1;
    Zmeya::HashMap<int, float> hashMap2;

    Zmeya::HashMap<Zmeya::String, float> shashMap1;
    Zmeya::HashMap<Zmeya::String, float> shashMap1a;
    Zmeya::HashMap<int, Zmeya::String> shashMap2;
    Zmeya::HashMap<int, Zmeya::String> shashMap2a;

    Zmeya::HashMap<Zmeya::String, Zmeya::String> shashMap3;
    Zmeya::HashMap<Zmeya::String, Zmeya::String> shashMap3a;
};

TEST(ZmeyaTestSuite1, HashMapTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<HMapNode> root = blob->emplace_back<HMapNode>();

    std::unordered_map<int, float> testMap = {{3, 7.0f}, {4, 17.0f}, {9, 79.0f}, {11, 13.0f}, {77, 13.0f}};
    blob->assign(root->hashMap1, testMap);
    EXPECT_EQ(root->hashMap1.size(), 5);
    EXPECT_FLOAT_EQ(root->hashMap1.find(3, -1.0f), 7.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(4, -1.0f), 17.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(9, -1.0f), 79.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(11, -1.0f), 13.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(12, -1.0f), -1.0f);
    EXPECT_FLOAT_EQ(root->hashMap1.find(77, 13.0f), 13.0f);
    EXPECT_EQ(root->hashMap1.find(99), nullptr);
    EXPECT_NE(root->hashMap1.find(3), nullptr);
    EXPECT_EQ(root->hashMap1.contains(99), false);
    EXPECT_EQ(root->hashMap1.contains(3), true);

    blob->assign(root->hashMap2, {{1, -1.0f}, {1, -10.0f}, {2, -2.0f}, {3, -3.0f}, {3, -30.0f}});
    EXPECT_EQ(root->hashMap2.size(), 3);
    EXPECT_FLOAT_EQ(root->hashMap2.find(1, 0.0f), -1.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(2, 0.0f), -2.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(3, 0.0f), -3.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(4, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(5, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(6, 0.0f), 0.0f);

    std::unordered_map<std::string, float> sMap1 = {{"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}};
    blob->assign(root->shashMap1, sMap1);
    EXPECT_EQ(root->shashMap1.size(), 3);
    EXPECT_FLOAT_EQ(root->shashMap1.find("one", 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(root->shashMap1.find("two", 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(root->shashMap1.find("three", 0.0f), 3.0f);

    blob->assign(root->shashMap1a, {{"five", -5.0f}, {"six", -6.0f}});
    EXPECT_EQ(root->shashMap1a.size(), 2);
    EXPECT_FLOAT_EQ(root->shashMap1a.find("five", 0.0f), -5.0f);
    EXPECT_FLOAT_EQ(root->shashMap1a.find("six", 0.0f), -6.0f);
    EXPECT_FLOAT_EQ(root->shashMap1a.find("seven", 0.0f), 0.0f);

    std::unordered_map<int, std::string> sMap2 = {{1, "one"}, {2, "two"}, {3, "three"}, {5, "five"}, {10, "ten"}};
    blob->assign(root->shashMap2, sMap2);
    EXPECT_EQ(root->shashMap2.size(), 5);
    EXPECT_STREQ(root->shashMap2.find(1, ""), "one");
    EXPECT_STREQ(root->shashMap2.find(2, ""), "two");
    EXPECT_STREQ(root->shashMap2.find(3, ""), "three");
    EXPECT_STREQ(root->shashMap2.find(5, ""), "five");
    EXPECT_STREQ(root->shashMap2.find(10, ""), "ten");
    EXPECT_STREQ(root->shashMap2.find(13, "-none-"), "-none-");

    blob->assign(root->shashMap2a, {{5, "five"}, {7, "seven"}, {7, "ten"}});
    EXPECT_EQ(root->shashMap2a.size(), 2);
    EXPECT_STREQ(root->shashMap2a.find(5, ""), "five");
    EXPECT_STREQ(root->shashMap2a.find(7, ""), "seven");
    EXPECT_STREQ(root->shashMap2a.find(6, "-none-"), "-none-");

    std::unordered_map<std::string, std::string> sMap3 = {{"1", "one"}, {"2", "two"}, {"3", "three"}, {"5", "five"}, {"10", "ten"}};
    blob->assign(root->shashMap3, sMap3);
    EXPECT_EQ(root->shashMap3.size(), 5);
    EXPECT_STREQ(root->shashMap3.find("1", ""), "one");
    EXPECT_STREQ(root->shashMap3.find("2", ""), "two");
    EXPECT_STREQ(root->shashMap3.find("3", ""), "three");
    EXPECT_STREQ(root->shashMap3.find("5", ""), "five");
    EXPECT_STREQ(root->shashMap3.find("10", ""), "ten");
    EXPECT_STREQ(root->shashMap3.find("13", "-none-"), "-none-");

    blob->assign(root->shashMap3a, {{"5", "five"}, {"7", "seven"}, {"7", "ten"}});
    EXPECT_EQ(root->shashMap3a.size(), 2);
    EXPECT_STREQ(root->shashMap3a.find("5", ""), "five");
    EXPECT_STREQ(root->shashMap3a.find("7", ""), "seven");
    EXPECT_STREQ(root->shashMap3a.find("6", "-none-"), "-none-");

    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);
    const HMapNode* obj = (const HMapNode*)(bytesCopy.data());

    EXPECT_EQ(obj->shashMap1.size(), 3);
    EXPECT_FLOAT_EQ(obj->shashMap1.find("one", 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(obj->shashMap1.find("two", 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(obj->shashMap1.find("three", 0.0f), 3.0f);

    EXPECT_EQ(obj->shashMap1a.size(), 2);
    EXPECT_FLOAT_EQ(obj->shashMap1a.find("five", 0.0f), -5.0f);
    EXPECT_FLOAT_EQ(obj->shashMap1a.find("six", 0.0f), -6.0f);
    EXPECT_FLOAT_EQ(obj->shashMap1a.find("seven", 0.0f), 0.0f);

    EXPECT_EQ(obj->shashMap2a.size(), 2);
    EXPECT_STREQ(obj->shashMap2a.find(5, ""), "five");
    EXPECT_STREQ(obj->shashMap2a.find(7, ""), "seven");
    EXPECT_STREQ(obj->shashMap2a.find(6, "-none-"), "-none-");

    EXPECT_EQ(obj->shashMap2.size(), 5);
    EXPECT_STREQ(obj->shashMap2.find(1, ""), "one");
    EXPECT_STREQ(obj->shashMap2.find(2, ""), "two");
    EXPECT_STREQ(obj->shashMap2.find(3, ""), "three");
    EXPECT_STREQ(obj->shashMap2.find(5, ""), "five");
    EXPECT_STREQ(obj->shashMap2.find(10, ""), "ten");
    EXPECT_STREQ(obj->shashMap2.find(13, "-none-"), "-none-");

    EXPECT_EQ(obj->shashMap3.size(), 5);
    EXPECT_STREQ(obj->shashMap3.find("1", ""), "one");
    EXPECT_STREQ(obj->shashMap3.find("2", ""), "two");
    EXPECT_STREQ(obj->shashMap3.find("3", ""), "three");
    EXPECT_STREQ(obj->shashMap3.find("5", ""), "five");
    EXPECT_STREQ(obj->shashMap3.find("10", ""), "ten");
    EXPECT_STREQ(obj->shashMap3.find("13", "-none-"), "-none-");

    EXPECT_EQ(obj->shashMap3a.size(), 2);
    EXPECT_STREQ(obj->shashMap3a.find("5", ""), "five");
    EXPECT_STREQ(obj->shashMap3a.find("7", ""), "seven");
    EXPECT_STREQ(obj->shashMap3a.find("6", "-none-"), "-none-");

    EXPECT_EQ(obj->hashMap1.size(), 5);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(3, -1.0f), 7.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(4, -1.0f), 17.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(9, -1.0f), 79.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(11, -1.0f), 13.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(12, -1.0f), -1.0f);
    EXPECT_FLOAT_EQ(obj->hashMap1.find(77, 13.0f), 13.0f);
    EXPECT_EQ(obj->hashMap1.find(99), nullptr);
    EXPECT_NE(obj->hashMap1.find(3), nullptr);
    EXPECT_EQ(obj->hashMap1.contains(99), false);
    EXPECT_EQ(obj->hashMap1.contains(3), true);

    EXPECT_EQ(root->hashMap2.size(), 3);
    EXPECT_FLOAT_EQ(root->hashMap2.find(1, 0.0f), -1.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(2, 0.0f), -2.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(3, 0.0f), -3.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(4, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(5, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(root->hashMap2.find(6, 0.0f), 0.0f);
}

struct IterTestNode
{
    Zmeya::Array<int> arr;
    Zmeya::HashSet<int> set;
    Zmeya::HashMap<int, int> map;
};

static void iteratorTest(const IterTestNode* root)
{
    std::vector<int> temp;

    EXPECT_EQ(root->arr.size(), 11);
    temp.clear();
    temp.resize(root->arr.size(), 0);
    for (const int& val : root->arr)
    {
        temp[val] += 1;
    }
    for (size_t i = 0; i < temp.size(); i++)
    {
        EXPECT_EQ(temp[i], 1);
    }

    EXPECT_EQ(root->set.size(), 6);
    temp.clear();
    temp.resize(root->set.size(), 0);
    for (const int& val : root->set)
    {
        temp[val] += 1;
    }
    for (size_t i = 0; i < temp.size(); i++)
    {
        EXPECT_EQ(temp[i], 1);
    }

    EXPECT_EQ(root->map.size(), 3);
    temp.clear();
    temp.resize(root->map.size() * 2, 0);
    for (const Zmeya::Pair<int, int>& val : root->map)
    {
        temp[val.first] += 1;
        temp[val.second] += 1;
    }
    for (size_t i = 0; i < temp.size(); i++)
    {
        EXPECT_EQ(temp[i], 1);
    }
}

TEST(ZmeyaTestSuite1, IteratorsTest)
{
    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<IterTestNode> root = blob->emplace_back<IterTestNode>();

    blob->assign(root->arr, {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0});
    blob->assign(root->set, {0, 1, 4, 3, 5, 2});
    blob->assign(root->map, {{0, 1}, {3, 2}, {4, 5}});

    iteratorTest(root.get());

    Zmeya::Span<char> bytes = blob->finalize();
    std::vector<char> bytesCopy = copyBytes(bytes);
    const IterTestNode* obj = (const IterTestNode*)(bytesCopy.data());

    iteratorTest(obj);
}
