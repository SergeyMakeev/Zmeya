#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

#include <cstdio>
#include <unordered_map>
#include <unordered_set>

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

struct ObjectFlat
{
    std::string name;
    Vec2 position;
};

struct Object : public Node
{
    zm::Pointer<Object> parent;
    Vec2 position;

    Object& operator=(const ObjectFlat& o)
    {
        name = o.name;
        position = o.position;
        return *this;
    }
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

    EXPECT_EQ(root->objects.size(), std::size_t(6));
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

    EXPECT_EQ(root->hashSet.size(), std::size_t(3));
    EXPECT_TRUE(root->hashSet.contains("one"));
    EXPECT_TRUE(root->hashSet.contains("two"));
    EXPECT_TRUE(root->hashSet.contains("three"));

    EXPECT_EQ(root->hashMap.size(), std::size_t(3));
    EXPECT_FLOAT_EQ(root->hashMap.find("1", 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("2", 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(root->hashMap.find("3", 0.0f), 3.0f);
}

static void generateTestFile(const char* fileName)
{
    std::vector<std::string> objectNames = {"root", "test1", "floor", "window", "arrow", "door"};

    zm::BlobBuffer bytesCopy = zm::write_scope<SimpleFileTestRoot>(
        [&objectNames](zm::BlobWriter<SimpleFileTestRoot>& w)
        {
            zm::ArenaRef<SimpleFileTestRoot> root = w.root();
            root->magic = 0x59454D5A;

            std::vector<ObjectFlat> objs;
            objs.reserve(objectNames.size());
            for (size_t i = 0; i < objectNames.size(); i++)
            {
                objs.push_back(ObjectFlat{objectNames[i], Vec2(float(i), float(i + 4))});
            }
        root->objects = objs;

        for (size_t i = 1; i < root->objects.size(); i++)
        {
            Object* cur = root->objects.get_element_ptr_unsafe_can_be_relocated(i);
            Object* prev = root->objects.get_element_ptr_unsafe_can_be_relocated(i - 1);
            cur->parent = prev;
        }

        std::unordered_set<std::string> hs = {"one", "two", "three"};
        root->hashSet = hs;

        std::unordered_map<std::string, float> hm = {{"1", 1.0f}, {"2", 2.0f}, {"3", 3.0f}};
        root->hashMap = hm;

            validate(root.transient_ptr());
        },
        32);

    EXPECT_TRUE((bytesCopy.size() % 32) == 0);

    const SimpleFileTestRoot* rootCopy = (const SimpleFileTestRoot*)(bytesCopy.data());
    validate(rootCopy);

    FILE* file = fopen(fileName, "wb");
    ASSERT_TRUE(file != nullptr);
    fwrite(bytesCopy.data(), bytesCopy.size(), 1, file);
    fclose(file);
}

TEST(ZmeyaTestSuite, SimpleFileTest)
{
    const char* fileName = "test.zm";
    generateTestFile(fileName);

    std::vector<char> content;

    FILE* file = fopen(fileName, "rb");
    ASSERT_TRUE(file != nullptr);
    fseek(file, 0L, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    content.resize(size_t(fileSize));
    fread(content.data(), size_t(fileSize), 1, file);
    fclose(file);

    const SimpleFileTestRoot* fileRoot = (const SimpleFileTestRoot*)(content.data());
    validate(fileRoot);
}
