# 7. Topology Snapshot Contract

Date: 2026-09-13

## Status

Accepted as the Phase 1 topology design.

Structural topology validation, copy-on-install client state, deterministic internal topology resolution, deterministic bounded planning over resolved candidates, internal monotonic topology-age policy evaluation, and callable local submission acceptance are implemented. Accepted requests materialize their selected validator identities and endpoints so later topology replacement does not retarget them. Discovery, adaptive routing, retries, transport attempts, and transport remain unimplemented.

## Context

Routing requires leader and validator contact information, but transaction submission must not be hard-wired to one RPC, streaming, or external discovery mechanism.

The delivery core therefore needs a transport-independent input representation for current routing information.

## Decision

Phase 1 defines a caller-supplied topology snapshot boundary.

A discovery component obtains external cluster information and converts it into the public topology representation. The delivery library consumes that representation without needing to know how it was obtained.

A built-in discovery implementation may be added separately.

## Snapshot Semantics

A topology snapshot represents one coherent routing view supplied to the library.

It must not require the route planner to combine independently changing caller-owned arrays after installation.

On successful installation, the implementation must internalize the data it requires or otherwise establish an explicit ownership contract before returning.

The simplest Phase 1 contract is copy-on-install: caller-provided snapshot memory may be released or reused after the update call returns.

Installation is transactional:

1. validate the complete caller-supplied snapshot;
2. construct a complete temporary owned snapshot;
3. obtain the local monotonic receipt time;
4. replace the currently installed snapshot only after all preceding steps succeed.

Validation failure, allocation failure, copy failure, or failure to obtain the required monotonic receipt time must leave the previously installed snapshot unchanged.

Client creation, client destruction, and topology installation initially require exclusive access to the client handle. This contract does not yet declare concurrent access to one client safe.

## Owned Representation

The implementation internalizes only the ABI prefix it understands for each topology record.

Caller records may use larger `struct_size` values and larger array strides because compatible ABI revisions may append fields. An implementation compiled against an older compatible prefix:

- validates the caller element against the supplied stride;
- copies the fields in the ABI prefix it understands;
- does not interpret unknown appended bytes;
- does not require unknown appended bytes to remain alive after installation;
- may normalize its owned arrays to the implementation-known native element size and stride.

This keeps internal topology state independent from caller memory without assigning semantics to ABI fields the implementation does not understand.

## Array Layout and Extensibility

Topology arrays carry an explicit byte stride in addition to their pointer and element count.

The stride is the byte distance between consecutive elements. This is required because validator, endpoint, association, and leader records use `struct_size` and may grow by appending fields in compatible ABI revisions.

A consumer must not assume that its own `sizeof(element_type)` is the stride of a caller-supplied array.

For each non-empty array:

- the pointer must be non-null;
- the stride must be large enough for the ABI prefix understood by the consumer;
- each element must declare a `struct_size` large enough for that prefix;
- an element `struct_size` must not exceed the supplied stride.

This preserves append-only structure evolution without making topology arrays ambiguous.

## Snapshot Identity

Each installed snapshot receives or carries a monotonically comparable generation value.

Generation identifies ordering of topology updates within one client instance. It is not a Solana slot and must not be interpreted as one.

The first snapshot installed into a client may carry any `uint64_t` generation value, including zero.

After a snapshot has been installed, a replacement must carry a strictly greater generation. An equal or lower generation is stale and must be rejected with `SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE`.

No generation reset operation is defined in Phase 1.

## Freshness

Snapshot freshness and chain slot information are distinct.

The library records local receipt time for an installed snapshot using its own monotonic time source.

Routing policy may reject or degrade behavior when topology age exceeds configured policy.

Caller-supplied wall-clock timestamps must not be used as the sole basis for monotonic expiry calculations.

## Validator Identity

Validator identity is represented independently from network endpoints.

The identity representation must be able to carry one Solana validator public key without requiring textual base58 conversion inside the routing hot path.

A canonical 32-byte binary identity representation is appropriate for the public topology boundary.

## Endpoint Representation

Transport endpoints must not expose platform `sockaddr` structures in the stable public ABI.

The public endpoint representation must encode:

- address family
- address bytes
- port
- transport role or class when necessary

IPv4 and IPv6 must have canonical explicit representations.

Port values must have one documented host-or-network-order convention at the ABI boundary.

The implementation converts that representation to platform socket structures internally.

## Identity-to-Endpoint Mapping

A topology snapshot may associate one validator identity with multiple endpoints.

The architecture must also permit multiple validator identities to reference the same transport endpoint.

Therefore:

- validator identity is routing metadata
- transport endpoint identity is connection-pool metadata
- neither is used as an implicit substitute for the other

## Leader Information

Leader information associates slot or slot-range routing context with validator identity.

The route planner may construct a plausible-leader frontier from more than one leader entry.

The public submission API must not require the caller to identify exactly one validator target.

## Snapshot Validation

Installation must reject structurally invalid topology, including invalid array bounds, unsupported endpoint address families, malformed identity lengths, and internally inconsistent references.

Semantic validation must not silently invent missing leader or endpoint information.

## Discovery Separation

The topology snapshot API is not an RPC API.

RPC polling, streaming updates, Geyser consumers, test fixtures, or application-specific discovery systems may all produce snapshots through adapters.

This keeps the transaction-delivery core independently testable.

## Consequences

Phase 1 routing tests can operate entirely on deterministic synthetic snapshots.

Transport and discovery implementations can evolve independently of the caller-facing submission ABI.
