#include "TestHelper.h"

#include <cstdio>
#include <new>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#elif defined(__has_include)
#if __has_include(<sys/mman.h>)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define ZMEYA_TEST_HAVE_POSIX_MMAP 1
#endif
#endif

#ifndef ZMEYA_TEST_HAVE_POSIX_MMAP
#define ZMEYA_TEST_HAVE_POSIX_MMAP 0
#endif

namespace zmeya_test_mapped_detail
{
struct Platform
{
#if defined(_WIN32)
    HANDLE hFile{INVALID_HANDLE_VALUE};
    HANDLE hMap{NULL};
    void* view{nullptr};
#elif ZMEYA_TEST_HAVE_POSIX_MMAP
    void* addr{MAP_FAILED};
    size_t len{0};
#else
    std::vector<char> buf;
#endif
};
} // namespace zmeya_test_mapped_detail

static zmeya_test_mapped_detail::Platform* as_plat(void* p) noexcept
{
    return reinterpret_cast<zmeya_test_mapped_detail::Platform*>(p);
}

void zmeya_test::MappedReadOnlyFile::release() noexcept
{
    if (plat_ == nullptr)
    {
        return;
    }
    zmeya_test_mapped_detail::Platform* plat = as_plat(plat_);
#if defined(_WIN32)
    if (plat->view)
    {
        UnmapViewOfFile(plat->view);
        plat->view = nullptr;
    }
    if (plat->hMap)
    {
        CloseHandle(plat->hMap);
        plat->hMap = NULL;
    }
    if (plat->hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(plat->hFile);
        plat->hFile = INVALID_HANDLE_VALUE;
    }
#elif ZMEYA_TEST_HAVE_POSIX_MMAP
    if (plat->addr != MAP_FAILED && plat->addr != nullptr && plat->len > 0)
    {
        munmap(plat->addr, plat->len);
        plat->addr = MAP_FAILED;
        plat->len = 0;
    }
#else
    plat->buf.clear();
#endif
    plat->~Platform();
    ::operator delete(plat);
    plat_ = nullptr;
    bytes_ = nullptr;
    size_ = 0;
}

zmeya_test::MappedReadOnlyFile::~MappedReadOnlyFile()
{
    release();
}

bool zmeya_test::MappedReadOnlyFile::try_open(const char* path) noexcept
{
    release();
    void* raw = operator new(sizeof(zmeya_test_mapped_detail::Platform), std::nothrow);
    if (raw == nullptr)
    {
        return false;
    }
    plat_ = raw;
    new (plat_) zmeya_test_mapped_detail::Platform();
    zmeya_test_mapped_detail::Platform* plat = as_plat(plat_);

#if defined(_WIN32)
    plat->hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (plat->hFile == INVALID_HANDLE_VALUE)
    {
        release();
        return false;
    }
    LARGE_INTEGER fileSizeInBytes{};
    if (!GetFileSizeEx(plat->hFile, &fileSizeInBytes))
    {
        release();
        return false;
    }
    plat->hMap = CreateFileMapping(
        plat->hFile, nullptr, PAGE_READONLY | SEC_COMMIT, fileSizeInBytes.HighPart, fileSizeInBytes.LowPart, nullptr);
    if (plat->hMap == NULL)
    {
        release();
        return false;
    }
    plat->view = MapViewOfFile(plat->hMap, FILE_MAP_READ, 0, 0, size_t(fileSizeInBytes.QuadPart));
    if (plat->view == nullptr)
    {
        release();
        return false;
    }
    bytes_ = plat->view;
    size_ = size_t(fileSizeInBytes.QuadPart);
    return true;

#elif ZMEYA_TEST_HAVE_POSIX_MMAP
    const int fd = ::open(path, O_RDONLY);
    if (fd < 0)
    {
        release();
        return false;
    }
    struct stat st {};
    if (::fstat(fd, &st) != 0)
    {
        ::close(fd);
        release();
        return false;
    }
    const size_t sz = size_t(st.st_size);
    if (sz == 0)
    {
        ::close(fd);
        release();
        return false;
    }
    void* p = ::mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    if (p == MAP_FAILED)
    {
        release();
        return false;
    }
    plat->addr = p;
    plat->len = sz;
    bytes_ = p;
    size_ = sz;
    return true;

#else
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr)
    {
        release();
        return false;
    }
    if (std::fseek(file, 0L, SEEK_END) != 0)
    {
        std::fclose(file);
        release();
        return false;
    }
    const long fileSize = std::ftell(file);
    if (fileSize < 0)
    {
        std::fclose(file);
        release();
        return false;
    }
    if (std::fseek(file, 0L, SEEK_SET) != 0)
    {
        std::fclose(file);
        release();
        return false;
    }
    plat->buf.resize(size_t(fileSize));
    if (fileSize > 0)
    {
        if (std::fread(plat->buf.data(), size_t(fileSize), 1, file) != size_t(1))
        {
            std::fclose(file);
            release();
            return false;
        }
    }
    std::fclose(file);
    bytes_ = plat->buf.data();
    size_ = plat->buf.size();
    return true;
#endif
}
