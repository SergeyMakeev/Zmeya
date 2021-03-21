#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct Vec2
{
    float x;
    float y;

    Vec2() = default;
    Vec2(float _x, float _y)
        : x(_x)
        , y(_y)
    {
    }
};

struct Node
{
    zm::String name;
};

// as long as inheritance = aggregation it is supported (but beware of different compilers paddings!)
struct Object : public Node
{
    zm::Pointer<Object> parent;
    Vec2 position;
};

struct SimpleFileTestRoot
{
    uint32_t magic;
    zm::Array<Object> objects;
    zm::HashSet<zm::String> hashSet;
    zm::HashMap<zm::String, float> hashMap;
};

static void validate(const SimpleFileTestRoot* root)
{
    EXPECT_EQ(root->magic, 0x59454D5Au);

    EXPECT_EQ(root->objects.size(), 6);
    EXPECT_EQ(root->objects[0].name, "root");
    EXPECT_EQ(root->objects[1].name, "test1");
    EXPECT_EQ(root->objects[2].name, "floor");
    EXPECT_EQ(root->objects[3].name, "window");
    EXPECT_EQ(root->objects[4].name, "arrow");
    EXPECT_EQ(root->objects[5].name, "door");

    for (size_t i = 0; i < root->objects.size(); i++)
    {
        const Object& object = root->objects[i];
        EXPECT_FLOAT_EQ(object.position.x, float(i));
        EXPECT_FLOAT_EQ(object.position.y, float(i + 4));
        if (i == 0)
        {
            EXPECT_TRUE(object.parent == nullptr);
        }
        else
        {
            EXPECT_TRUE(object.parent.get() == &root->objects[i - 1]);
        }
    }

    EXPECT_EQ(root->hashSet.size(), 3);
    EXPECT_TRUE(root->hashSet.contains("one"));
    EXPECT_TRUE(root->hashSet.contains("two"));
    EXPECT_TRUE(root->hashSet.contains("three"));

    EXPECT_EQ(root->hashMap.size(), 3);
    EXPECT_FLOAT_EQ(root->hashMap.find("1", 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("2", 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("3", 0.0f), 3.0f);
}

static void generateTestFile(const char* fileName)
{
    std::vector<std::string> objectNames = {"root", "test1", "floor", "window", "arrow", "door"};

    std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create();
    zm::BlobPtr<SimpleFileTestRoot> root = blobBuilder->allocate<SimpleFileTestRoot>();
    root->magic = 0x59454D5A;
    blobBuilder->resizeArray(root->objects, 6);
    for (size_t i = 0; i < root->objects.size(); i++)
    {
        Object& object = root->objects[i];
        blobBuilder->copyTo(object.name, objectNames[i]);
        object.position = Vec2(float(i), float(i + 4));

        if (i > 0)
        {
            const Object& parentObject = root->objects[i - 1];
            blobBuilder->assignTo(object.parent, parentObject);
        }
    }

    blobBuilder->copyTo(root->hashSet, {"one", "two", "three"});
    blobBuilder->copyTo(root->hashMap, {{"1", 1.0f}, {"2", 2.0f}, {"3", 3.0f}});

    validate(root.get());

    zm::Span<char> bytes = blobBuilder->finalize(32);
    EXPECT_TRUE((bytes.size % 32) == 0);

    std::vector<char> bytesCopy = utils::copyBytes(bytes);
    const SimpleFileTestRoot* rootCopy = (const SimpleFileTestRoot*)(bytesCopy.data());
    validate(rootCopy);

    FILE* file = fopen(fileName, "wb");
    ASSERT_TRUE(file != nullptr);
    fwrite(bytes.data, bytes.size, 1, file);
    fclose(file);
}

TEST(ZmeyaTestSuite, SimpleFileTest)
{
    const char* fileName = "test.zmy";
    generateTestFile(fileName);

    std::vector<char> content;

    // read file
    FILE* file = fopen(fileName, "rb");
    ASSERT_TRUE(file != nullptr);
    fseek(file, 0L, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    content.resize(fileSize);
    fread(content.data(), fileSize, 1, file);
    fclose(file);

    const SimpleFileTestRoot* fileRoot = (const SimpleFileTestRoot*)(content.data());
    validate(fileRoot);
}
