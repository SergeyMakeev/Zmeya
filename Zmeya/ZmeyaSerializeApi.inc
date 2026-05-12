/*

**Serialization API surface**

`assign`, `deep_copy`, incremental hash helpers, container member bodies, and `BlobWriter` shims are
split across `ZmeyaSerializeApi*.inc` fragments included below.

**Include order (read / debug)**

1. `ZmeyaSerializeApiAssignDetail.inc` -- `assign_pointer`, `assign_string_std` / `assign_string_cstr`, `deep_copy_into_offset`
2. `ZmeyaBuilderHashChainNodesGrow.inc` -- hash chain node slab growth (`hash_chain_nodes_array_grow_*`)
3. `ZmeyaSerializeApiAssign.inc` -- `assign` overloads into `zm::` fields
4. `ZmeyaSerializeApiIncremental.inc` -- free `hashmap_insert`, `array_push_back`, etc.
5. `ZmeyaSerializeApiInstMembers.inc` -- `Array` / `String` / `HashMap` / `HashSet` member bodies
6. `ZmeyaSerializeApiBlobWriter.inc` -- `BlobWriter<TRoot>::*` forwarding to (2)-(4)

*/

#include "ZmeyaSerializeApiAssignDetail.inc"
#include "ZmeyaBuilderHashChainNodesGrow.inc"
#include "ZmeyaSerializeApiAssign.inc"
#include "ZmeyaSerializeApiIncremental.inc"
#include "ZmeyaSerializeApiInstMembers.inc"
#include "ZmeyaSerializeApiBlobWriter.inc"
