#include "TestHelper.h"
#include "Zmeya.h"
#include "gtest/gtest.h"

struct SimpleRoot
{
    zm::String description;
};

TEST(ZmeyaTestSuite, MinimalBuilderTest)
{
    // Create builder - automatically sets TLS context
    auto builder = zm::Builder::create();
    
    // Allocate root
    SimpleRoot* root = builder->allocate_root<SimpleRoot>();
    
    // Test basic string assignment
    std::string srcDesc = "Test";
    zm::assign(root->description, srcDesc);
    
    // Get result
    zm::Span<char> bytes = builder->finalize();
    
    // Validate the serialized data
    const SimpleRoot* fileRoot = reinterpret_cast<const SimpleRoot*>(bytes.data);
    
    EXPECT_EQ(fileRoot->description, "Test");
}
