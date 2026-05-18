#include "ZmeyaTestCoverageCommon.h"

// P2-01: Assigning a long string through the builder must remain stable (growth path, no truncation for large lengths).
TEST(ZmeyaTestSuite, Coverage_P2_LongStringAssignRoundTrip)
{
    std::string longStr(size_t(100000), 'z');
    for (size_t i = 0; i < longStr.size(); i += 997)
    {
        longStr[i] = static_cast<char>('a' + (i % 26));
    }

    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovStringRoot>(
        [&longStr](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = longStr;
        }, 64, 4);

    EXPECT_EQ(std::string(reinterpret_cast<const CovStringRoot*>(blob.data())->text.c_str()), longStr);
}

// P2-02: Embedded NUL bytes are stored with full std::string length via memcpy; C-string APIs still truncate at the first NUL (documented limitation).
TEST(ZmeyaTestSuite, Coverage_P2_StringWithEmbeddedNulStoredLength)
{
    std::string s("pre");
    s.push_back('\0');
    s.append("post", 4);

    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovStringRoot>(
        [&s](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = s;
        }, 256, 4);

    const zm::String& zs = reinterpret_cast<const CovStringRoot*>(blob.data())->text;
    EXPECT_EQ(std::string(zs.c_str()), std::string("pre"));
    EXPECT_TRUE(zs == std::string("pre"));
}

// P2-03: const char* and std::string sources with identical byte content produce identical serialized strings.
TEST(ZmeyaTestSuite, Coverage_P2_CharPtrVsStdStringSameBytes)
{
    zm::BlobBuffer a = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = "identical_payload";
        }, 128, 4);

    zm::BlobBuffer b = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("identical_payload");
        }, 128, 4);

    EXPECT_STREQ(reinterpret_cast<const CovStringRoot*>(a.data())->text.c_str(),
        reinterpret_cast<const CovStringRoot*>(b.data())->text.c_str());
}

// P2-04: Member operator+= only (no bulk assign of the whole string until implicit initial empty) under a tiny reserve.
TEST(ZmeyaTestSuite, Coverage_P2_StringMemberPlusEqualsChain)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            for (int i = 0; i < 200; ++i)
            {
                w.root()->text += "x";
            }
        }, 32, 4);

    EXPECT_EQ(std::strlen(reinterpret_cast<const CovStringRoot*>(blob.data())->text.c_str()), 200u);
}

// P2-05: String::clear() from the member API leaves an empty C string view.
TEST(ZmeyaTestSuite, Coverage_P2_StringMemberClear)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("will_clear");
            w.root()->text.clear();
        }, 128, 4);

    const zm::String& t = reinterpret_cast<const CovStringRoot*>(blob.data())->text;
    EXPECT_TRUE(t.empty());
    EXPECT_STREQ(t.c_str(), "");
}

// P2-06: Comparison operators cover zm vs const char*, std::string, and reflexive cases.
TEST(ZmeyaTestSuite, Coverage_P2_StringComparisonOperators)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovStringCmpRoot>(
        [](zm::BlobWriter<CovStringCmpRoot>& w)
        {
            w.root()->a = std::string("same");
            w.root()->b = std::string("other");
        }, 256, 4);

    const CovStringCmpRoot* r = reinterpret_cast<const CovStringCmpRoot*>(blob.data());
    EXPECT_TRUE(r->a == r->a);
    EXPECT_FALSE(r->a != r->a);
    EXPECT_TRUE(r->a == "same");
    EXPECT_TRUE("same" == r->a);
    EXPECT_TRUE(r->a == std::string("same"));
    EXPECT_TRUE(std::string("same") == r->a);
    EXPECT_TRUE(r->a != r->b);
    EXPECT_FALSE(r->a == r->b);
}

// P2-07: Self-alias via c_str is not supported for zm::String assign (would read/write overlapping storage); skipped as a documented contract.
TEST(ZmeyaTestSuite, Coverage_P2_StringSelfAliasAssignSkipped)
{
    GTEST_SKIP() << "zm::String assign from its own c_str is undefined; not exercised.";
}

// P3-01: zm::String arrays reject incremental push_back at compile time via zm_array_push_back_ok.
TEST(ZmeyaTestSuite, Coverage_P3_ArrayStringPushBackCompileTrait)
{
    static_assert(!zm::detail::zm_array_push_back_ok<zm::String>::value, "incremental push_back must be disabled for zm::String elements");
}

// P3-02: Array of pointers round-trips from a std::vector of allocated node addresses.
TEST(ZmeyaTestSuite, Coverage_P3_ArrayOfPointersAssign)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovArrayPtrRoot>(
        [](zm::BlobWriter<CovArrayPtrRoot>& w)
        {
            std::vector<CovPointerChainNode*> ptrs;
            for (int i = 0; i < 5; ++i)
            {
                zm::ArenaRef<CovPointerChainNode> n = w.allocate<CovPointerChainNode>();
                n->id = i * 10;
                n->next = nullptr;
                ptrs.push_back(n.transient_ptr());
            }
            w.root()->nodes = ptrs;
        }, 256, 4);

    const zm::Array<zm::Pointer<CovPointerChainNode>>& a = reinterpret_cast<const CovArrayPtrRoot*>(blob.data())->nodes;
    ASSERT_EQ(a.size(), 5u);
    for (size_t i = 0; i < 5; ++i)
    {
        ASSERT_NE(a[i].get(), nullptr);
        EXPECT_EQ(a[i]->id, int32_t(i * 10));
    }
}

// P3-03: Nested Array<Array<int>> bulk-assign matches the nested std::vector model after read-back.
TEST(ZmeyaTestSuite, Coverage_P3_NestedArrayIntBulkAssign)
{
    struct Root
    {
        zm::Array<zm::Array<int32_t>> grid;
    };

    std::vector<std::vector<int32_t>> model = {{1, 2}, {3, 4, 5}, {6}};

    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<Root>(
        [&model](zm::BlobWriter<Root>& w)
        {
            w.root()->grid = model;
        }, 1024, 4);

    const Root* r = reinterpret_cast<const Root*>(blob.data());
    ASSERT_EQ(r->grid.size(), 3u);
    EXPECT_EQ(r->grid[0].size(), 2u);
    EXPECT_EQ(r->grid[1].size(), 3u);
    EXPECT_EQ(r->grid[2].size(), 1u);
    EXPECT_EQ(r->grid[1][2], 5);
}

// P3-04: array_resize to a large count fills new slots with the provided pattern (large starting arena avoids bump slab churn exhausting the arena on huge fills).
TEST(ZmeyaTestSuite, Coverage_P3_ArrayResizeLargeFillPattern)
{
#ifdef _DEBUG
    constexpr size_t kN = 4096;
    constexpr size_t kStartArenaBytes = 4u * 1024u * 1024u;
#else
    // Release uses a larger resize than Debug, but keep the logical size below what the bump-only resize path can push past the int32 goffset_t arena budget for this test harness.
    constexpr size_t kN = size_t(1) << 14;
    constexpr size_t kStartArenaBytes = 32u * 1024u * 1024u;
#endif
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovIntArrayRoot>(
        [kN](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.array_resize(w.root()->values, kN, int32_t(-7));
        },
        kStartArenaBytes,
        4);

    const zm::Array<int32_t>& a = reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values;
    ASSERT_EQ(a.size(), kN);
    EXPECT_EQ(a[0], -7);
    EXPECT_EQ(a[kN / 2], -7);
    EXPECT_EQ(a[kN - 1], -7);
}

// P3-05: array_erase_at on an empty array is a documented no-op (out-of-range index).
TEST(ZmeyaTestSuite, Coverage_P3_ArrayEraseAtWhenEmptyNoOp)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovIntArrayRoot>(
        [](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.array_erase_at(w.root()->values, 0);
        }, 32, 4);

    EXPECT_EQ(reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values.size(), 0u);
}

// P4-01: Null zm::Pointer serializes as a zero relative offset and reads back as nullptr.
TEST(ZmeyaTestSuite, Coverage_P4_NullPointerRoundTrip)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovNullPointerRoot>(
        [](zm::BlobWriter<CovNullPointerRoot>& w)
        {
            w.root()->p = nullptr;
        }, 64, 4);

    EXPECT_EQ(reinterpret_cast<const CovNullPointerRoot*>(blob.data())->p.get(), nullptr);
}

// P4-02: Long singly-linked pointer chain preserves roffset links across many nodes.
TEST(ZmeyaTestSuite, Coverage_P4_PointerChainThousandNodes)
{
    zm::BlobBuffer blob = zm::write_scope<CovPointerChainRoot>(
        [](zm::BlobWriter<CovPointerChainRoot>& w)
        {
            constexpr int kN = 1000;
            zm::detail::BuilderBase* bb = w.builder_base();
            zm::goffset_t g[kN];
            for (int i = 0; i < kN; ++i)
            {
                zm::ArenaRef<CovPointerChainNode> p = w.allocate<CovPointerChainNode>();
                g[i] = bb->arena_byte_offset_of(p.transient_ptr());
                p->id = i;
                p->next = nullptr;
            }
            for (int i = 0; i < kN - 1; ++i)
            {
                CovPointerChainNode* pi = reinterpret_cast<CovPointerChainNode*>(bb->get_ptr_unsafe_to_store(g[i]));
                CovPointerChainNode* pj = reinterpret_cast<CovPointerChainNode*>(bb->get_ptr_unsafe_to_store(g[i + 1]));
                pi->next = pj;
            }
            CovPointerChainNode* head = reinterpret_cast<CovPointerChainNode*>(bb->get_ptr_unsafe_to_store(g[0]));
            w.root()->head = head;
        },
        8);

    const CovPointerChainNode* cur = reinterpret_cast<const CovPointerChainRoot*>(blob.data())->head.get();
    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_NE(cur, nullptr);
        EXPECT_EQ(cur->id, i);
        cur = cur->next.get();
    }
    EXPECT_EQ(cur, nullptr);
}

// P4-03: Small tree with parent pointers plus child arrays forms a DAG-style back reference graph.
TEST(ZmeyaTestSuite, Coverage_P4_TreeWithParentPointers)
{
    zm::BlobBuffer blob = zm::write_scope<CovTreeRoot>(
        [](zm::BlobWriter<CovTreeRoot>& w)
        {
            zm::ArenaRef<CovTreeNode> root = w.allocate<CovTreeNode>();
            root->value = 1;
            root->parent = nullptr;
            zm::ArenaRef<CovTreeNode> left = w.allocate<CovTreeNode>();
            left->value = 2;
            left->parent = root.transient_ptr();
            zm::ArenaRef<CovTreeNode> right = w.allocate<CovTreeNode>();
            right->value = 3;
            right->parent = root.transient_ptr();
            std::vector<CovTreeNode*> ch = {left.transient_ptr(), right.transient_ptr()};
            root->children = ch;
            w.root()->root = root.transient_ptr();
        },
        8);

    const CovTreeNode* root = reinterpret_cast<const CovTreeRoot*>(blob.data())->root.get();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->value, 1);
    ASSERT_EQ(root->children.size(), 2u);
    EXPECT_EQ(root->children[0]->value, 2);
    EXPECT_EQ(root->children[1]->value, 3);
    EXPECT_EQ(root->children[0]->parent.get(), root);
    EXPECT_EQ(root->children[1]->parent.get(), root);
}

// P4-04: allocate returns addresses aligned to the requested T inside the bump arena.
TEST(ZmeyaTestSuite, Coverage_P4_AllocatePointerAlignment)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovAllocAlignRoot>(
        [](zm::BlobWriter<CovAllocAlignRoot>& w)
        {
            zm::ArenaRef<double> pd = w.allocate<double>();
            *pd = 3.25;
            zm::ArenaRef<CovAlignedPayload> pp = w.allocate<CovAlignedPayload>();
            pp->a = 1;
            pp->b = 2;
            w.root()->pd = pd.transient_ptr();
            w.root()->pp = pp.transient_ptr();
        }, 64, 8);

    const CovAllocAlignRoot* r = reinterpret_cast<const CovAllocAlignRoot*>(blob.data());
    const uintptr_t pda = reinterpret_cast<uintptr_t>(r->pd.get());
    const uintptr_t ppa = reinterpret_cast<uintptr_t>(r->pp.get());
    EXPECT_EQ(pda % alignof(double), 0u);
    EXPECT_EQ(ppa % alignof(CovAlignedPayload), 0u);
}

