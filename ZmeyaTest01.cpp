#include "gtest/gtest.h"

/*
namespace Memory
{

size_t mallocCount = 0;
size_t freeCount = 0;

struct Header
{
    void* p;
    size_t size;
    size_t magic;
};

const size_t kMinValidAlignment = 4;

void* malloc(size_t bytesCount, size_t alignment)
{
    mallocCount++;
    if (alignment < kMinValidAlignment)
    {
        alignment = kMinValidAlignment;
    }
    void* p;
    void** p2;
    size_t offset = alignment - 1 + sizeof(Header);
    if ((p = (void*)std::malloc(bytesCount + offset)) == NULL)
    {
        return NULL;
    }
    p2 = (void**)(((size_t)(p) + offset) & ~(alignment - 1));

    Header* h = reinterpret_cast<Header*>(reinterpret_cast<char*>(p2) - sizeof(Header));
    h->p = p;
    h->size = bytesCount;
    h->magic = size_t(0x13061979);
    return p2;
}

void mfree(void* p)
{
    freeCount++;
    if (!p)
    {
        return;
    }
    Header* h = reinterpret_cast<Header*>(reinterpret_cast<char*>(p) - sizeof(Header));
    ASSERT_EQ(h->magic, size_t(0x13061979));
    std::free(h->p);
}

} // namespace Memory

#define ZMEYA_ALLOC(sizeInBytes, alignment) Memory::malloc(sizeInBytes, alignment)
#define ZMEYA_FREE(ptr) Memory::mfree(ptr)
*/

#include "TestHelper.h"
#include "Zmeya.h"

struct SimpleTestRoot
{
    float a;
    uint32_t b;
    uint16_t c;
    int8_t d;
    uint32_t arr[32];
};

static void validate(const SimpleTestRoot* root)
{
    EXPECT_FLOAT_EQ(root->a, 13.0f);
    EXPECT_EQ(root->b, 1979u);
    EXPECT_EQ(root->c, 6);
    EXPECT_EQ(root->d, -9);
    for (size_t i = 0; i < 32; i++)
    {
        EXPECT_EQ(root->arr[i], uint32_t(i + 3));
    }
}

TEST(ZmeyaTestSuite, SimpleTest)
{
    std::vector<char> bytesCopy;
    {
        // create blob
        std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create(1);

        // allocate structure
        zm::BlobPtr<SimpleTestRoot> root = blobBuilder->allocate<SimpleTestRoot>();

        // fill with data
        root->a = 13.0f;
        root->b = 1979;
        root->c = 6;
        root->d = -9;
        for (size_t i = 0; i < 32; i++)
        {
            root->arr[i] = uint32_t(i + 3);
        }

        validate(root.get());

        // finalize blob
        zm::Span<char> bytes = blobBuilder->finalize();

        // copy resulting bytes
        bytesCopy = utils::copyBytes(bytes);

        // fill original memory with 0xff
        std::memset(bytes.data, 0xFF, bytes.size);
    }

    // "deserialize" and test results
    const SimpleTestRoot* rootCopy = (const SimpleTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}

struct Desc
{
    zm::String name;
    float v1;
    uint32_t v2;
};

struct TestRoot
{
    zm::Array<Desc> arr;
};

TEST(ZmeyaTestSuite, SimpleTest2)
{
    const std::vector<std::string> names = {"apple",   "banana",  "orange",  "castle",  "dragon",  "flower",  "guitar",
                                            "hockey",  "island",  "jungle",  "kingdom", "library", "monster", "notable",
                                            "oceanic", "painter", "quarter", "rescue",  "seventh", "trivial", "umbrella",
                                            "village", "warrior", "xenial",  "yonder",  "zephyr"};

/*
    Memory::mallocCount = 0;
    Memory::freeCount = 0;
    EXPECT_EQ(Memory::mallocCount, size_t(0));
    EXPECT_EQ(Memory::freeCount, size_t(0));
*/

    std::vector<char> blob;
    {
        std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create(1);
        zm::BlobPtr<TestRoot> root = blobBuilder->allocate<TestRoot>();

        blobBuilder->resizeArray(root->arr, names.size());
        EXPECT_EQ(root->arr.size(), names.size());

        for (size_t i = 0; i < names.size(); i++)
        {
            zm::BlobPtr<Desc> desc = blobBuilder->getArrayElement(root->arr, i);
            blobBuilder->copyTo(desc->name, names[i].c_str());
            const char* s1 = root->arr[i].name.c_str();
            const char* s2 = names[i].c_str();
            EXPECT_STREQ(s1, s2);
            desc->v1 = (float)(i);
            desc->v2 = (uint32_t)(i);
        }
        zm::Span<char> bytes = blobBuilder->finalize();
        blob = std::vector<char>(bytes.data, bytes.data + bytes.size);
    }

/*
    EXPECT_GT(Memory::mallocCount, size_t(0));
    EXPECT_GT(Memory::freeCount, size_t(0));
    EXPECT_EQ(Memory::mallocCount, Memory::freeCount);
*/

    // validate
    const TestRoot* rootCopy = (const TestRoot*)(blob.data());
    EXPECT_EQ(rootCopy->arr.size(), names.size());

    for (size_t i = 0; i < names.size(); i++)
    {
        const Desc& desc = rootCopy->arr[i];
        EXPECT_STREQ(desc.name.c_str(), names[i].c_str());
        EXPECT_FLOAT_EQ(desc.v1, (float)(i));
        EXPECT_EQ(desc.v2, (uint32_t)(i));
    }

/*
    EXPECT_EQ(Memory::mallocCount, Memory::freeCount);
*/
}