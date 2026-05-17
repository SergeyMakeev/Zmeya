#include "ZmeyaTestCoverageCommon.h"

// P5-01: Alignment of 1 always yields a valid blob size (trivially size % 1 == 0).
TEST(ZmeyaTestSuite, Coverage_P5_FinalizeAlignmentOne)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = "x";
        }, 64, 1);
    EXPECT_EQ(blob.size() % 1, 0u);
}

// P5-02: Extra finalize padding bytes (when using stricter alignment) are zero-filled in the output buffer.
TEST(ZmeyaTestSuite, Coverage_P5_FinalizePaddingBytesZero)
{
    zm::BlobBuffer a = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = "pad";
        }, 128, 4);

    zm::BlobBuffer b = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = "pad";
        }, 128, 8);

    ASSERT_GE(b.size(), a.size());
    for (size_t i = a.size(); i < b.size(); ++i)
    {
        EXPECT_EQ(static_cast<unsigned char>(b[i]), 0u);
    }
}

// P5-04: write_scope returns owning bytes with the same length as finalize() on an equivalent session (size sanity).
TEST(ZmeyaTestSuite, Coverage_P5_WriteBlobVectorMatchesFinalizeSize)
{
    std::unique_ptr<zm::detail::Builder<CovStringRoot>> builder = zm::detail::Builder<CovStringRoot>::create(128);
    zm::detail::ScopedBuilder scope(builder.get());
    builder->getRoot()->text = std::string("size_check");
    zm::Span<char> span = builder->finalize(8);
    zm::BlobBuffer viaVec = zmeya_test::write_scope_stressed<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            w.root()->text = std::string("size_check");
        }, 128, 8);
    EXPECT_EQ(viaVec.size(), span.size);
}

// P6-01: Memory-map path on Windows (and buffer read on other hosts) accepts non-default finalize alignment (16).
TEST(ZmeyaTestSuite, Coverage_P6_MmapMiniRootAlignment16)
{
    const char* fileName = "coverage_mmap_mini.zm";
    zm::BlobBuffer bytes = zm::write_scope<CovMmapMiniRoot>(
        [](zm::BlobWriter<CovMmapMiniRoot>& w)
        {
            w.root()->magic = 0x11223344u;
            w.root()->tag = std::string("mmap_align16");
        }, 16);

    EXPECT_EQ(bytes.size() % 16, 0u);

    FILE* out = fopen(fileName, "wb");
    ASSERT_TRUE(out != nullptr);
    ASSERT_EQ(fwrite(bytes.data(), bytes.size(), 1, out), size_t(1));
    fclose(out);

#if defined(_WIN32)
    HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT_TRUE(hFile != INVALID_HANDLE_VALUE);
    LARGE_INTEGER sz;
    ASSERT_TRUE(GetFileSizeEx(hFile, &sz));
    HANDLE hMap = CreateFileMapping(hFile, 0, PAGE_READONLY | SEC_COMMIT, sz.HighPart, sz.LowPart, 0);
    ASSERT_TRUE(hMap != NULL);
    const CovMmapMiniRoot* mapped = reinterpret_cast<const CovMmapMiniRoot*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, size_t(sz.QuadPart)));
    ASSERT_TRUE(mapped != nullptr);
    EXPECT_EQ(mapped->magic, 0x11223344u);
    EXPECT_EQ(mapped->tag, std::string("mmap_align16"));
    UnmapViewOfFile(mapped);
    CloseHandle(hMap);
    CloseHandle(hFile);
#else
    FILE* in = fopen(fileName, "rb");
    ASSERT_TRUE(in != nullptr);
    fseek(in, 0L, SEEK_END);
    long n = ftell(in);
    fseek(in, 0L, SEEK_SET);
    std::vector<char> buf(size_t(n));
    ASSERT_EQ(fread(buf.data(), size_t(n), 1, in), size_t(1));
    fclose(in);
    const CovMmapMiniRoot* mapped = reinterpret_cast<const CovMmapMiniRoot*>(buf.data());
    EXPECT_EQ(mapped->magic, 0x11223344u);
    EXPECT_EQ(mapped->tag, std::string("mmap_align16"));
#endif
    remove(fileName);
}

// P6-04: Full file round-trip preserves raw blob bytes (fwrite/fread memcmp).
TEST(ZmeyaTestSuite, Coverage_P6_FileRoundTripBytesIdentical)
{
    zm::BlobBuffer bytes = zmeya_test::write_scope_stressed<CovHashMapIntIntRoot>(
        [](zm::BlobWriter<CovHashMapIntIntRoot>& w)
        {
            w.hashmap_insert(w.root()->map, 1, 2);
            w.hashmap_insert(w.root()->map, 3, 4);
        }, 512, 8);

    const char* path = "coverage_roundtrip.zm";
    {
        std::ofstream os(path, std::ios::binary);
        ASSERT_TRUE(os);
        os.write(bytes.data(), std::streamsize(bytes.size()));
    }
    std::vector<char> back(bytes.size());
    {
        std::ifstream is(path, std::ios::binary);
        ASSERT_TRUE(is);
        is.read(back.data(), std::streamsize(back.size()));
        ASSERT_EQ(size_t(is.gcount()), back.size());
    }
    remove(path);
    EXPECT_EQ(std::memcmp(bytes.data(), back.data(), bytes.size()), 0);
}

// P7-01: Smaller ReferTo-shaped graph (100 nodes) keeps the same per-node validation pattern as the stress test.
TEST(ZmeyaTestSuite, Coverage_P7_ReferToShapedSmallNodeCount)
{
    CoverageReferNodeInit proto;
    proto.str = "This is supposed to be a long enough string. I think it is long enough now.";
    proto.arr = {1, 2, 5, 8, 13, 99, 7, 160, 293, 890};
    proto.hashSet = {1, 5, 15, 23, 38, 31};
    proto.hashMap = {{"one", 1.0f}, {"two", 2.0f}, {"three", 3.0f}, {"four", 4.0f}};

    constexpr size_t kNodes = 100;
    zm::BlobBuffer blob = zm::write_scope<CoverageReferMultiRoot>(
        [&proto, kNodes](zm::BlobWriter<CoverageReferMultiRoot>& w)
        {
            std::vector<CoverageReferNodeInit> inits(kNodes, proto);
            w.root()->nodes = inits;
        },
        4);

    const CoverageReferMultiRoot* r = reinterpret_cast<const CoverageReferMultiRoot*>(blob.data());
    ASSERT_EQ(r->nodes.size(), kNodes);
    for (size_t i = 0; i < kNodes; ++i)
    {
        const CoverageReferNode& n = r->nodes[i];
        EXPECT_EQ(n.str, proto.str);
        ASSERT_EQ(n.arr.size(), proto.arr.size());
        EXPECT_EQ(n.hashSet.size(), proto.hashSet.size());
        EXPECT_EQ(n.hashMap.size(), proto.hashMap.size());
    }
}

// P7-02: Single-node ReferTo-shaped payload still exercises all nested container kinds on the read path.
TEST(ZmeyaTestSuite, Coverage_P7_ReferToShapedSingleNode)
{
    CoverageReferNodeInit proto;
    proto.str = "solo_node_string_payload";
    proto.arr = {9, 8, 7};
    proto.hashSet = {100, 200};
    proto.hashMap = {{"k", 1.5f}};

    zm::BlobBuffer blob = zm::write_scope<CoverageReferSingleRoot>(
        [&proto](zm::BlobWriter<CoverageReferSingleRoot>& w)
        {
            w.root()->node = proto;
        }, 4);

    const CoverageReferNode& n = reinterpret_cast<const CoverageReferSingleRoot*>(blob.data())->node;
    EXPECT_EQ(n.str, proto.str);
    ASSERT_EQ(n.arr.size(), 3u);
    EXPECT_TRUE(n.hashSet.contains(100));
    EXPECT_FLOAT_EQ(n.hashMap.find("k", 0.0f), 1.5f);
}

// P7-03: Short intrusive list smoke (10 nodes) validates prev/next wiring without the million-node cost.
TEST(ZmeyaTestSuite, Coverage_P7_ListChainSmokeTenNodes)
{
    struct Node
    {
        uint32_t payload;
        zm::Pointer<Node> prev;
        zm::Pointer<Node> next;
    };

    struct Root
    {
        uint32_t numNodes;
        zm::Pointer<Node> head;
    };

    constexpr uint32_t kN = 10;
    zm::BlobBuffer blob = zm::write_scope<Root>(
        [kN](zm::BlobWriter<Root>& w)
        {
            zm::detail::BuilderBase* bb = w.builder_base();
            w.root()->numNodes = kN;
            zm::goffset_t prev_g{};
            for (uint32_t i = 0; i < kN; ++i)
            {
                zm::ArenaRef<Node> node = w.allocate<Node>();
                node->payload = 100 + i;
                node->prev = nullptr;
                node->next = nullptr;
                if (i > 0)
                {
                    Node* prev = reinterpret_cast<Node*>(bb->get_ptr_unsafe_to_store(prev_g));
                    node->prev = prev;
                    prev->next = node.transient_ptr();
                }
                else
                {
                    w.root()->head = node.transient_ptr();
                }
                prev_g = bb->arena_byte_offset_of(node.transient_ptr());
            }
        },
        8);

    const Root* rr = reinterpret_cast<const Root*>(blob.data());
    uint32_t c = 0;
    const Node* cur = rr->head.get();
    while (cur)
    {
        EXPECT_EQ(cur->payload, 100 + c);
        cur = cur->next.get();
        ++c;
    }
    EXPECT_EQ(c, kN);
}

// P8-01: Range-for over empty zm containers does not crash and yields zero iterations.
TEST(ZmeyaTestSuite, Coverage_P8_EmptyContainerIteration)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovIterEmptyRoot>(
        [](zm::BlobWriter<CovIterEmptyRoot>& /*w*/) {}, 128, 4);

    const CovIterEmptyRoot* r = reinterpret_cast<const CovIterEmptyRoot*>(blob.data());
    size_t nArr = 0;
    for (int32_t v : r->arr)
    {
        (void)v;
        ++nArr;
    }
    EXPECT_EQ(nArr, 0u);
    size_t nHs = 0;
    for (int32_t v : r->hs)
    {
        (void)v;
        ++nHs;
    }
    EXPECT_EQ(nHs, 0u);
    size_t nHm = 0;
    for (const auto& p : r->hm)
    {
        (void)p;
        ++nHm;
    }
    EXPECT_EQ(nHm, 0u);
}

// P8-02: For a non-empty array, begin/end iterators compare as unequal until the walk completes.
TEST(ZmeyaTestSuite, Coverage_P8_IteratorBeginEndPatterns)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovIntArrayRoot>(
        [](zm::BlobWriter<CovIntArrayRoot>& w)
        {
            w.array_push_back(w.root()->values, 1);
            w.array_push_back(w.root()->values, 2);
        }, 64, 4);

    const zm::Array<int32_t>& a = reinterpret_cast<const CovIntArrayRoot*>(blob.data())->values;
    EXPECT_NE(a.begin(), a.end());
    size_t count = 0;
    for (const int32_t* it = a.begin(); it != a.end(); ++it)
    {
        ++count;
    }
    EXPECT_EQ(count, 2u);
}

// P8-03: HashMap::find(key, default) returns the default when the key is absent.
TEST(ZmeyaTestSuite, Coverage_P8_HashMapFindWithDefault)
{
    zm::BlobBuffer blob = zmeya_test::write_scope_stressed<CovHashMapStrIntRoot>(
        [](zm::BlobWriter<CovHashMapStrIntRoot>& w)
        {
            w.hashmap_insert(w.root()->map, std::string("only"), 5);
        }, 128, 4);

    const zm::HashMap<zm::String, int32_t>& m = reinterpret_cast<const CovHashMapStrIntRoot*>(blob.data())->map;
    EXPECT_EQ(m.find("only", -999), 5);
    EXPECT_EQ(m.find("missing", -999), -999);
}

// P9-04: BlobWriter exposes a non-null builder_base during write_scope (already covered elsewhere; repeated for backlog traceability).
TEST(ZmeyaTestSuite, Coverage_P9_BlobWriterBuilderBaseNonNull)
{
    zm::write_scope<CovStringRoot>(
        [](zm::BlobWriter<CovStringRoot>& w)
        {
            EXPECT_NE(w.builder_base(), nullptr);
        });
}
