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
    std::vector<char> bytesCopy = zm::build<SimpleTestRoot>(
        [](SimpleTestRoot* root)
        {
            root->a = 13.0f;
            root->b = 1979;
            root->c = 6;
            root->d = -9;
            for (size_t i = 0; i < 32; i++)
            {
                root->arr[i] = uint32_t(i + 3);
            }

            validate(root);
        });

    const SimpleTestRoot* rootCopy = (const SimpleTestRoot*)(bytesCopy.data());
    validate(rootCopy);
}

struct Desc
{
    zm::String name;
    float v1;
    uint32_t v2;
    
    // Assignment operator for automatic conversion from TempDesc
    template<typename TempType>
    Desc& operator=(const TempType& temp)
    {
        name = temp.name;
        v1 = temp.v1;
        v2 = temp.v2;
        return *this;
    }
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

    struct TempDesc
    {
        std::string name;
        float v1;
        uint32_t v2;
    };

    std::vector<char> blob = zm::build<TestRoot>(
        [&](TestRoot* root)
        {
            std::vector<TempDesc> tempDescs;
            tempDescs.reserve(names.size());

            for (size_t i = 0; i < names.size(); i++)
            {
                TempDesc desc{};
                desc.name = names[i];
                desc.v1 = (float)(i);
                desc.v2 = (uint32_t)(i);
                tempDescs.push_back(desc);
            }

            root->arr = tempDescs;

            EXPECT_EQ(root->arr.size(), names.size());

            for (size_t i = 0; i < names.size(); i++)
            {
                const char* s1 = root->arr[i].name.c_str();
                const char* s2 = names[i].c_str();
                EXPECT_STREQ(s1, s2);
                EXPECT_FLOAT_EQ(root->arr[i].v1, (float)(i));
                EXPECT_EQ(root->arr[i].v2, (uint32_t)(i));
            }
        });

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