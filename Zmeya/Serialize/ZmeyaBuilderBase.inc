/*

**`detail::BuilderBase` implementation body**

Arena compaction, registry/patching, allocation, strings, arrays, and finalize live in separate
`ZmeyaBuilderBase*.inc` fragments so navigation matches subsystems without changing the single class.

**Include order (read / debug)**

1. `ZmeyaBuilderBaseArena.inc` -- arena bytes, `contains_pointer`, dead ranges
2. `ZmeyaBuilderBaseRegistry.inc` -- `roffset` slot registry, patching
3. `ZmeyaBuilderBaseAlloc.inc` -- `alloc_aligned`, placement helpers
4. `ZmeyaBuilderBaseCtor.inc` -- construction helpers
5. `ZmeyaBuilderBaseStrings.inc` -- string slab ops (`string_append`, `string_clear`, ...)
6. `ZmeyaBuilderBaseArrays.inc` -- array slab ops
7. `ZmeyaBuilderBaseFinalize.inc` -- `finalize`, compaction

Chained hash insert/rehash/erase live in `ZmeyaBuilderHashChain.inc` (included from `ZmeyaBuilderBase.h`, not here).

*/

class BuilderBase
{
  public:
    std::vector<char, BufferAllocator<char, ZMEYA_MAX_ALIGN>> data;

#if defined(_DEBUG) || defined(ZMEYA_DEBUG_ARENA_EPOCH)
    uint64_t arena_realloc_epoch_ = 0;
#endif

    struct PrivateToken
    {
    };

    std::unordered_map<goffset_t, goffset_t> roffset_slot_targets_;

    std::unordered_map<goffset_t, size_t> string_blob_phys_bytes_;

    // Builder-only: tracks node slab byte size (may exceed nodes.numElements*sizeof) and first-slab hints.
    std::unordered_map<goffset_t, size_t> hash_chain_nodes_phys_bytes_;
    std::unordered_map<goffset_t, size_t> hash_chain_nodes_first_alloc_hint_;

    // Builder-only: backing store bytes for Array slabs (may exceed numElements*sizeof(T)); keys Array header goffsets.
    std::unordered_map<goffset_t, size_t> array_slab_phys_bytes_;

    std::vector<std::pair<goffset_t, size_t>> dead_ranges_;

#include "ZmeyaBuilderBaseArena.inc"
#include "ZmeyaBuilderBaseRegistry.inc"
#include "ZmeyaBuilderBaseAlloc.inc"
#include "ZmeyaBuilderBaseCtor.inc"
#include "ZmeyaBuilderBaseStrings.inc"
#include "ZmeyaBuilderBaseArrays.inc"
#include "ZmeyaBuilderBaseFinalize.inc"

};
