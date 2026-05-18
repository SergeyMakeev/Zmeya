/*

**Read this first (serialize hash chains)**

Three supported ways to mutate blob fields during a session (see `AGENTS.md` for realloc rules):

1. **`zm::write_scope<TRoot>`** plus **`zm::BlobWriter<TRoot>`** methods (`w.hashmap_insert`, `w.array_push_back`, ...). Prefer this in new code so the writer is explicit.

2. **`zm::assign(...)`** and free **`zm::hashmap_insert` / `hashset_insert` / ...`** overloads that use **thread-local** `detail::BuilderBase*` (`require_tls_builder()`).

3. **`zm::assign(detail::BuilderBase&, ...)`** and **`zm::hashmap_insert(builder, ...)`** when TLS is not the right scope (nested `ScopedBuilder`, tests).

**Option C chained hash (incremental builder)**

Design background: `.audit/performance-audit.md` (repo). Dense `nodes[]`, `uint32_t` links with `ZMEYA_HASH_CHAIN_NIL`, `buckets[].head`, prepend into chains, free list on erase, rehash when `live_count_ >= bucket_count` (effective max load factor 1.0), initial 8 buckets. Wire format stays chained through finalize (no flatten-to-flat-items pass).

**Arena growth and `hm` / `hs` references**

`HashMap& hm` and `HashSet& hs` point into `BuilderBase::data`. Any `alloc_aligned` may reallocate that vector, so those references can dangle for the rest of the call. Always capture `hm_g` / `hs_g` up front, then re-derive `p` via `get_ptr_unsafe_to_store(hm_g)` after growth. Never call `arena_byte_offset_of(&hm...)` or `arena_byte_offset_of(&hs...)` after a growth step in the same function unless you re-bound `hm`/`hs` from `p` (prefer `&p->nodes` etc.).

**Reviewer checklist (after edits)**

- After any step that can grow the arena, rebind `p` with `get_ptr_unsafe_to_store(hm_g)` / `hs_g` before using `hm`/`hs` or addresses derived from them.
- When walking chains, use `p->nodes` / `p->buckets` only from a freshly rebound `p`.

**File split**

Implementation is sharded into `ZmeyaBuilderHashChain{Helpers,Rehash,Ops,Reserve}.inc` for navigation; include order below matches the old monolith.

*/

#include "ZmeyaBuilderHashChainHelpers.inc"
#include "ZmeyaBuilderHashChainRehash.inc"
#include "ZmeyaBuilderHashChainOps.inc"
#include "ZmeyaBuilderHashChainReserve.inc"
