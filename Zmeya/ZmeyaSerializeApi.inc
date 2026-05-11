/*

**Serialization API surface**

`assign`, `deep_copy`, incremental hash helpers, container member bodies, and `BlobWriter` shims are
split across `ZmeyaSerializeApi*.inc` fragments included below.

*/

#include "ZmeyaSerializeApiAssignDetail.inc"
#include "ZmeyaSerializeApiAssign.inc"
#include "ZmeyaSerializeApiIncremental.inc"
#include "ZmeyaSerializeApiInstMembers.inc"
#include "ZmeyaSerializeApiBlobWriter.inc"
