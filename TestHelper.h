#pragma once
#include "Zmeya.h"
#include <cstddef>
#include <utility>

/*

**Test-only write_scope with a forced starting arena size**

Forwards to `zm::detail::write_scope_with_initial_buffer_bytes`. Use in tests to stress
`std::vector` reallocations; application code should use `zm::write_scope` instead.

*/

namespace zmeya_test
{
template <typename TRoot, typename Fn>
zm::BlobBuffer write_scope_stressed(Fn&& fn, size_t initialBufferBytes, size_t finalizeAlignment = 4)
{
    return zm::detail::write_scope_with_initial_buffer_bytes<TRoot>(std::forward<Fn>(fn), initialBufferBytes, finalizeAlignment);
}

/*

**Read-only file mapping for tests**

Uses real memory maps on Windows and on hosts where `<sys/mman.h>` exists (Linux, macOS, typical BSD).
Otherwise reads the file into a heap buffer so tests still run.

*/

class MappedReadOnlyFile
{
public:
    MappedReadOnlyFile() noexcept = default;
    ~MappedReadOnlyFile();

    MappedReadOnlyFile(const MappedReadOnlyFile&) = delete;
    MappedReadOnlyFile& operator=(const MappedReadOnlyFile&) = delete;

    bool try_open(const char* path) noexcept;

    const void* data() const noexcept { return bytes_; }
    size_t size() const noexcept { return size_; }

private:
    void release() noexcept;

    const void* bytes_{nullptr};
    size_t size_{0};
    void* plat_{nullptr};
};
} // namespace zmeya_test
