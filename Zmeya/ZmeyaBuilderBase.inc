/*

**`detail::BuilderBase` implementation body**

Arena compaction, registry/patching, allocation, strings, arrays, and finalize live in separate
`ZmeyaBuilderBase*.inc` fragments so navigation matches subsystems without changing the single class.

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
