#include "gtest/gtest.h"

#define ZMEYA_ENABLE_SERIALIZE_SUPPORT
#include "Zmeya.h"

struct Vec2
{
    float x;
    float y;

    Vec2() = default;
    Vec2(size_t ix, size_t iy)
        : x(float(ix))
        , y(float(iy))
    {
    }
};

struct Object
{
    Zmeya::String name;
    Zmeya::Pointer<Object> parent;
    Vec2 position;
};

struct Root
{
    uint32_t magic;
    Zmeya::Array<Object> objects;
};

static void testFileData(const Root* root) {}

static void generateTestFile(const char* fileName)
{
    std::vector<std::string> objectNames = {"root", "test1", "floor", "window", "arrow", "door"};

    std::shared_ptr<Zmeya::Blob> blob = Zmeya::Blob::create();
    Zmeya::BlobPtr<Root> root = blob->emplace_back<Root>();
    root->magic = 0x59454D5A;
    blob->resizeArray(root->objects, 6);
    for (size_t i = 0; i < root->objects.size(); i++)
    {
        struct Object& object = root->objects[i];
        blob->assign(object.name, objectNames[i]);
        object.position = Vec2(i, i + 4);

        if (i > 0)
        {
            struct Object& parentObject = root->objects[i - 1];
            blob->assign(object.parent, parentObject);
        }
    }

    testFileData(root.get());

    Zmeya::Span<char> bytes = blob->finalize(32);
    EXPECT_TRUE((bytes.size % 32) == 0);

    const Root* root2 = (const Root*)(bytes.data);
    testFileData(root.get());

    FILE* file = fopen(fileName, "wb");
    ASSERT_TRUE(file != nullptr);
    fwrite(bytes.data, bytes.size, 1, file);
    fclose(file);
}

TEST(ZmeyaTestSuite2, SimpleFileTest)
{
    generateTestFile("test.zm");

    //const Root* root2 = (const Root*)(bytes.data);
    //testFileData(root.get());
}
