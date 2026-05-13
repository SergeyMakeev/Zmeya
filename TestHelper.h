#pragma once
#include "Zmeya.h"
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
} // namespace zmeya_test
