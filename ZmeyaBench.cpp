#include "Zmeya.h"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{

struct RootMapIntInt
{
    zm::HashMap<int32_t, int32_t> map;
};

struct RootMapStringInt
{
    zm::HashMap<zm::String, int32_t> map;
};

struct RootSetInt
{
    zm::HashSet<int32_t> set;
};

struct RootArrayInt
{
    zm::Array<int32_t> values;
};

struct RootString
{
    zm::String text;
};

struct RootStrArray
{
    zm::Array<zm::String> items;
};

static void BM_HashMapInt32_BulkAssign(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::unordered_map<int32_t, int32_t> model;
    model.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        const int32_t k = static_cast<int32_t>(i);
        model[k] = static_cast<int32_t>(i * 3);
    }
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootMapIntInt>(
            [&model](zm::BlobWriter<RootMapIntInt>& w)
            {
                w.root()->map = model;
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapInt32_IncrementalInsert(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootMapIntInt>(
            [n](zm::BlobWriter<RootMapIntInt>& w)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    const int32_t k = static_cast<int32_t>(i);
                    w.hashmap_insert(w.root()->map, k, static_cast<int32_t>(i * 3));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapStringInt32_BulkAssign(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::unordered_map<std::string, int32_t> model;
    model.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        model["k" + std::to_string(i)] = static_cast<int32_t>(i);
    }
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootMapStringInt>(
            [&model](zm::BlobWriter<RootMapStringInt>& w)
            {
                w.root()->map = model;
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapStringInt32_IncrementalInsert(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootMapStringInt>(
            [n](zm::BlobWriter<RootMapStringInt>& w)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    w.hashmap_insert(w.root()->map, std::string("k" + std::to_string(i)), static_cast<int32_t>(i));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashSetInt32_BulkAssign(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::unordered_set<int32_t> model;
    model.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        model.insert(static_cast<int32_t>(i * 17 + 3));
    }
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootSetInt>(
            [&model](zm::BlobWriter<RootSetInt>& w)
            {
                w.root()->set = model;
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashSetInt32_IncrementalInsert(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootSetInt>(
            [n](zm::BlobWriter<RootSetInt>& w)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    w.hashset_insert(w.root()->set, static_cast<int32_t>(i * 17 + 3));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_ArrayInt32_BulkAssign(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::vector<int32_t> src(n);
    for (size_t i = 0; i < n; ++i)
    {
        src[i] = static_cast<int32_t>(static_cast<int>(i) * 5 - 101);
    }
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootArrayInt>(
            [&src](zm::BlobWriter<RootArrayInt>& w)
            {
                w.root()->values = src;
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_ArrayInt32_PushBack(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootArrayInt>(
            [n](zm::BlobWriter<RootArrayInt>& w)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    w.array_push_back(w.root()->values, static_cast<int32_t>(static_cast<int>(i) * 5 - 101));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapInt32_FindHit(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::unordered_map<int32_t, int32_t> model;
    model.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        const int32_t k = static_cast<int32_t>(i);
        model[k] = static_cast<int32_t>(i * 3);
    }
    zm::BlobBuffer blob = zm::write_blob<RootMapIntInt>(
        [&model](zm::BlobWriter<RootMapIntInt>& w)
        {
            w.root()->map = model;
        },
        4);
    const RootMapIntInt* root = reinterpret_cast<const RootMapIntInt*>(blob.data());
    size_t q = 0;
    for (auto _ : state)
    {
        const int32_t k = static_cast<int32_t>(q % n);
        benchmark::DoNotOptimize(root->map.find(k));
        ++q;
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_HashMapStringInt32_FindHit(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::unordered_map<std::string, int32_t> model;
    model.reserve(n);
    std::vector<std::string> keys;
    keys.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        keys.push_back("k" + std::to_string(i));
        model[keys.back()] = static_cast<int32_t>(i);
    }
    zm::BlobBuffer blob = zm::write_blob<RootMapStringInt>(
        [&model](zm::BlobWriter<RootMapStringInt>& w)
        {
            w.root()->map = model;
        },
        4);
    const RootMapStringInt* root = reinterpret_cast<const RootMapStringInt*>(blob.data());
    size_t q = 0;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(root->map.find(keys[q % n].c_str()));
        ++q;
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_StringRepeatedAssignFinalize(benchmark::State& state)
{
    const int rounds = static_cast<int>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootString>(
            [rounds](zm::BlobWriter<RootString>& w)
            {
                for (int i = 0; i < rounds; ++i)
                {
                    w.root()->text = std::string(256, static_cast<char>('A' + (i % 26)));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(rounds));
}

static void BM_HashMapInt32_IncrementalInsert_Reserved(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootMapIntInt>(
            [n](zm::BlobWriter<RootMapIntInt>& w)
            {
                w.hashmap_reserve_nodes(w.root()->map, n);
                for (size_t i = 0; i < n; ++i)
                {
                    const int32_t k = static_cast<int32_t>(i);
                    w.hashmap_insert(w.root()->map, k, static_cast<int32_t>(i * 3));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashSetInt32_IncrementalInsert_Reserved(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootSetInt>(
            [n](zm::BlobWriter<RootSetInt>& w)
            {
                w.hashset_reserve_nodes(w.root()->set, n);
                for (size_t i = 0; i < n; ++i)
                {
                    w.hashset_insert(w.root()->set, static_cast<int32_t>(i * 17 + 3));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapInt32_IncrementalEraseReinsert(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootMapIntInt>(
            [n](zm::BlobWriter<RootMapIntInt>& w)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    const int32_t k = static_cast<int32_t>(i);
                    w.hashmap_insert(w.root()->map, k, static_cast<int32_t>(i * 3));
                }
                for (size_t i = 0; i < n; i += 2)
                {
                    w.hashmap_erase(w.root()->map, static_cast<int32_t>(i));
                }
                const size_t need = (n + 1) / 2;
                for (size_t j = 0; j < need; ++j)
                {
                    const int32_t k = static_cast<int32_t>(1000000000 + static_cast<int32_t>(j));
                    w.hashmap_insert(w.root()->map, k, static_cast<int32_t>(j * 7));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapStringInt32_IncrementalInsert_Reserved(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootMapStringInt>(
            [n](zm::BlobWriter<RootMapStringInt>& w)
            {
                w.hashmap_reserve_nodes(w.root()->map, n);
                for (size_t i = 0; i < n; ++i)
                {
                    w.hashmap_insert(w.root()->map, std::string("k" + std::to_string(i)), static_cast<int32_t>(i));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapInt32_EraseHeavy(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootMapIntInt>(
            [n](zm::BlobWriter<RootMapIntInt>& w)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    w.hashmap_insert(w.root()->map, static_cast<int32_t>(i), static_cast<int32_t>(i * 3));
                }
                for (size_t i = 0; i < n; i += 2)
                {
                    w.hashmap_erase(w.root()->map, static_cast<int32_t>(i));
                }
                const size_t need = (n + 1) / 2;
                for (size_t j = 0; j < need; ++j)
                {
                    const int32_t k = static_cast<int32_t>(2000000000 + static_cast<int32_t>(j));
                    w.hashmap_insert(w.root()->map, k, static_cast<int32_t>(j * 11));
                }
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapInt32_IterateAll(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::unordered_map<int32_t, int32_t> model;
    model.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        const int32_t k = static_cast<int32_t>(i);
        model[k] = static_cast<int32_t>(i * 3);
    }
    zm::BlobBuffer blob = zm::write_blob<RootMapIntInt>(
        [&model](zm::BlobWriter<RootMapIntInt>& w)
        {
            w.root()->map = model;
        },
        4);
    const RootMapIntInt* root = reinterpret_cast<const RootMapIntInt*>(blob.data());
    for (auto _ : state)
    {
        size_t cnt = 0;
        for (auto it = root->map.begin(); it != root->map.end(); ++it)
        {
            ++cnt;
        }
        benchmark::DoNotOptimize(cnt);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

static void BM_HashMapInt32_FindMiss(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::unordered_map<int32_t, int32_t> model;
    model.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        const int32_t k = static_cast<int32_t>(i);
        model[k] = static_cast<int32_t>(i * 3);
    }
    zm::BlobBuffer blob = zm::write_blob<RootMapIntInt>(
        [&model](zm::BlobWriter<RootMapIntInt>& w)
        {
            w.root()->map = model;
        },
        4);
    const RootMapIntInt* root = reinterpret_cast<const RootMapIntInt*>(blob.data());
    size_t q = 0;
    for (auto _ : state)
    {
        const int32_t k = static_cast<int32_t>(700000000 + static_cast<int32_t>(q % 100000));
        benchmark::DoNotOptimize(root->map.find(k));
        ++q;
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_HashSetInt32_ContainsHit(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::unordered_set<int32_t> model;
    model.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        model.insert(static_cast<int32_t>(i * 17 + 3));
    }
    zm::BlobBuffer blob = zm::write_blob<RootSetInt>(
        [&model](zm::BlobWriter<RootSetInt>& w)
        {
            w.root()->set = model;
        },
        4);
    const RootSetInt* root = reinterpret_cast<const RootSetInt*>(blob.data());
    size_t q = 0;
    for (auto _ : state)
    {
        const int32_t k = static_cast<int32_t>((q % n) * 17 + 3);
        benchmark::DoNotOptimize(root->set.contains(k));
        ++q;
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_FinalizeManyStringRoffsets(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    std::vector<std::string> src;
    src.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        src.push_back("s" + std::to_string(i));
    }
    for (auto _ : state)
    {
        zm::BlobBuffer blob = zm::write_blob<RootStrArray>(
            [&src](zm::BlobWriter<RootStrArray>& w)
            {
                w.root()->items = src;
            },
            4);
        benchmark::DoNotOptimize(blob.data());
        benchmark::DoNotOptimize(blob.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
}

#if defined(_WIN32)
static void BM_Win32Mmap_HashMapInt32FindHit(benchmark::State& state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    if (n == 0)
    {
        state.SkipWithError("range must be positive");
        return;
    }
    std::unordered_map<int32_t, int32_t> model;
    model.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        const int32_t k = static_cast<int32_t>(i);
        model[k] = static_cast<int32_t>(i * 3);
    }
    zm::BlobBuffer init = zm::write_blob<RootMapIntInt>(
        [&model](zm::BlobWriter<RootMapIntInt>& w)
        {
            w.root()->map = model;
        },
        4);
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "zmeya_bench_mmap.bin";
    {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(init.data(), static_cast<std::streamsize>(init.size()));
    }
    HANDLE hf = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (hf == INVALID_HANDLE_VALUE)
    {
        state.SkipWithError("CreateFileW failed");
        return;
    }
    HANDLE hm = CreateFileMappingW(hf, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(hf);
    if (hm == nullptr)
    {
        (void)fs::remove(path);
        state.SkipWithError("CreateFileMappingW failed");
        return;
    }
    void* view = MapViewOfFile(hm, FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr)
    {
        CloseHandle(hm);
        (void)fs::remove(path);
        state.SkipWithError("MapViewOfFile failed");
        return;
    }
    const RootMapIntInt* root = reinterpret_cast<const RootMapIntInt*>(view);
    size_t q = 0;
    for (auto _ : state)
    {
        const int32_t k = static_cast<int32_t>(q % n);
        benchmark::DoNotOptimize(root->map.find(k));
        ++q;
    }
    UnmapViewOfFile(view);
    CloseHandle(hm);
    (void)fs::remove(path);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
#endif

} // namespace

BENCHMARK(BM_HashMapInt32_BulkAssign)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashMapInt32_IncrementalInsert)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashMapInt32_IncrementalInsert_Reserved)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashMapStringInt32_BulkAssign)->RangeMultiplier(8)->Range(8, 512);
BENCHMARK(BM_HashMapStringInt32_IncrementalInsert)->RangeMultiplier(8)->Range(8, 512);
BENCHMARK(BM_HashMapStringInt32_IncrementalInsert_Reserved)->RangeMultiplier(8)->Range(8, 512);
BENCHMARK(BM_HashMapInt32_IncrementalEraseReinsert)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashMapInt32_EraseHeavy)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashMapInt32_IterateAll)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashSetInt32_BulkAssign)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashSetInt32_IncrementalInsert)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashSetInt32_IncrementalInsert_Reserved)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_ArrayInt32_BulkAssign)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_ArrayInt32_PushBack)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashMapInt32_FindHit)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashMapInt32_FindMiss)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashSetInt32_ContainsHit)->RangeMultiplier(8)->Range(8, 4096);
BENCHMARK(BM_HashMapStringInt32_FindHit)->RangeMultiplier(8)->Range(8, 512);
BENCHMARK(BM_StringRepeatedAssignFinalize)->RangeMultiplier(4)->Range(4, 128);
BENCHMARK(BM_FinalizeManyStringRoffsets)->RangeMultiplier(4)->Range(8, 512);
#if defined(_WIN32)
BENCHMARK(BM_Win32Mmap_HashMapInt32FindHit)->RangeMultiplier(8)->Range(8, 4096);
#endif
