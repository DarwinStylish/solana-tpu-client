# Current Implementation Status

## Purpose

The repository name reflects the intended Solana TPU transaction-delivery project. The current codebase is a standalone pre-transport ingress prototype and must not be interpreted as a complete TPU client.

## Implemented

The current revision implements:

- a public C11 prototype API;
- a fixed-layout little-endian trade-event decoder;
- bounds and side-discriminator validation;
- a loopback-only non-blocking UDP receiver;
- callback-based event delivery;
- callback-driven lifecycle coordination with synchronization owned by the caller;
- localhost integration testing;
- a synthetic decoder microbenchmark;
- a standalone static-library build.

The public repository no longer requires private HFT engine headers or types to compile.

The repository also provides `include/solana/delivery.h` and `build/libsolana_delivery.a` for the emerging transaction-delivery boundary.

The delivery library currently implements structural topology validation, opaque client creation/destruction, transactional copy-on-install topology ownership, deterministic internal slot-to-topology-candidate resolution, an internal deterministic bounded route planner, monotonic topology-freshness admission, and callable local submission acceptance with owned request state. It does not implement adaptive routing, transport attempts, transport, polling, discovery, retries, or observation.

## Public/Private Boundary

The public prototype exposes schema-local decoded fields only.

It does not expose or depend on:

- private execution-event representations;
- private fixed-point types;
- private venue identifiers;
- private SPSC queues;
- trading strategy state;
- execution-engine state.

Translation from the public API into any private execution model belongs to the consuming application.

## Current Ingress API Contract

The fixed-layout decoder accepts exactly 33 bytes. It rejects null pointers, truncated inputs, oversized inputs, and unsupported side discriminators.

The instruction discriminator is currently exposed as an opaque schema field. This prototype does not define an authoritative set of instruction values and therefore does not reject values based on instruction semantics.

`solana_ingress_event_t` currently occupies 48 bytes. Its reserved bytes are zero-initialized by the decoder and must not be interpreted by consumers. Tests pin the current size and field offsets so accidental ABI drift is detected.

Event callbacks are synchronous. The event pointer is valid only while the callback is executing. Event and lifecycle contexts are caller-owned.

The local receiver returns zero after caller-requested shutdown. Operational failures return `-1`; the originating `errno` value is preserved across socket cleanup.

The API is pre-release and is not yet declared a permanent version-1 ABI.

## Delivery API and Topology State

The delivery header defines the Phase 1 ABI vocabulary together with callable topology validation and client/topology-state operations.

Its current contracts include:

- a 32-byte binary validator identity;
- explicit IPv4/IPv6 endpoint storage without platform `sockaddr` types;
- host-byte-order endpoint ports;
- separate validator and endpoint records with explicit association records;
- leader slot-range records;
- a topology view carrying generation and caller-observed slot context;
- opaque request and attempt identifiers;
- a 24-byte extensible submission-options record carrying maximum topology age and a bounded target limit;
- a 64-byte delivery-event envelope whose concrete event codes remain intentionally unfrozen.

The topology aggregate contains native process pointers to caller-owned arrays. `solana_delivery_client_install_topology` validates and internalizes the required data before returning success, after which the caller may reuse or release the supplied snapshot storage.

The owned representation retains only the ABI prefixes understood by this implementation and normalizes its internal arrays to implementation-known element sizes. Unknown compatible caller extensions are not interpreted or retained.

Topology arrays at the caller boundary carry explicit byte strides so append-only record extensions do not require consumers to assume their own `sizeof(element_type)` as the caller array layout.

`solana_delivery_topology_validate` currently validates:

- the known topology structure prefix;
- array base alignment;
- non-empty array pointer/count/stride consistency;
- stride compatibility with known element prefixes;
- element `struct_size` bounds;
- reserved fields;
- supported endpoint discriminators;
- nonzero endpoint ports;
- canonical IPv4 tail bytes;
- validator-to-endpoint index bounds;
- leader validator references;
- ordered leader slot ranges.

The validator is intentionally structural. It does not decide whether the topology contains a usable route or whether topology is fresh enough for a submission policy.

Topology installation adds stateful ordering semantics:

- the first installed snapshot may use any `uint64_t` generation, including zero;
- every later installation must use a strictly greater generation;
- equal or lower generations return `SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE`;
- successful installation records a local monotonic receipt time;
- validation, allocation/copy, and receipt-time acquisition complete before the installed snapshot is replaced;
- failure during any of those stages leaves the previously installed snapshot unchanged.

Tests exercise deep-copy ownership, caller-buffer independence, compatible extended-stride normalization, stale-generation rejection, allocation failure at each copy stage, and monotonic-clock failure. Sanitizer runs cover temporary-state cleanup on those failure paths.

An empty topology is structurally valid and may be installed. It does not imply that a route is available.

The internal topology resolver consumes only installed library-owned topology. For a requested slot it preserves leader-array order and, within each matching leader, validator-to-endpoint association order. It produces leader, validator, and endpoint indices without ranking, deduplication, fanout selection, freshness policy, retries, allocation, or network activity.

Resolution reports `SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE` when no topology is installed, no leader range contains the requested slot, or matching leaders yield no associated endpoints. Insufficient caller-provided candidate capacity is reported atomically with `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED` and the required count.

This resolver is internal implementation vocabulary and does not add a public routing ABI.

The internal route planner consumes only resolved candidates. It deduplicates by `(validator_index, endpoint_index)`, preserves the first occurrence and its leader provenance, preserves first-occurrence order, and selects at most a positive target limit.

The planner reports both the complete unique-target count and the bounded selected-target count. Insufficient output capacity is reported atomically with `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED` before any target output is written.

The planner performs no topology traversal, freshness evaluation, adaptive ranking, retry scheduling, connection management, transport work, allocation, or network activity. Its types and functions remain internal and do not add a public routing ABI.

Submission-policy evaluation is a separate internal stage. It validates a positive maximum topology age and target limit, requires an installed topology, samples the client monotonic clock once, accepts topology whose age is at most the configured maximum, returns `SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE` when that age is exceeded, and treats backward movement within the monotonic clock domain as an internal error. Generation and `current_slot` are not used as freshness clocks.

`solana_delivery_client_submit` composes that admission stage with deterministic resolution and bounded planning. The installed snapshot `current_slot` is used as the literal routing anchor, not as a freshness clock.

A successful local submission:

- copies the non-empty caller transaction byte range into library-owned storage;
- materializes the selected validator identities and endpoint records rather than depending only on replaceable topology indices;
- records the topology generation and routing slot used for planning;
- assigns a nonzero request identifier unique within the client lifetime;
- commits the request to library-owned state only after all preparation succeeds.

Submission preparation is failure-atomic. Argument rejection, stale or unavailable topology, planning failure, allocation failure, and request-ID exhaustion do not create a partially accepted request or consume an identifier.

Accepted requests are currently retained until client destruction. Client destruction releases their transaction storage, materialized target storage, and request records.

Submission success is local acceptance only. It does not create a transport attempt, send bytes, imply validator receipt, or claim landing or confirmation.

## Not Implemented

The current revision does not implement:

- transmission of locally accepted transactions to Solana TPU endpoints;
- QUIC client transport;
- TLS identity handling;
- `solana-tpu` ALPN negotiation;
- leader schedule tracking;
- TPU contact-information resolution;
- connection pooling or prewarming;
- stake-weighted QoS behavior;
- adaptive or transport-aware transaction routing;
- plausible-leader-frontier expansion beyond literal resolved slot matches;
- retry and backpressure policies;
- landing or confirmation observation;
- Agave/Firedancer TPU-ingress conformance;
- kernel-bypass networking.

## Prototype Parser Scope

The current decoder operates on one project-specific 33-byte little-endian trade-event layout.

It is not:

- a Solana transaction decoder;
- a general Borsh implementation;
- the Solana TPU wire protocol.

The receiver maintains a local datagram sequence counter. Successfully decoded events carry the counter value assigned when their datagram was received; rejected datagrams may therefore create gaps. The value is not validator- or exchange-assigned.

## Benchmark Scope

The current benchmark repeatedly decodes one preallocated synthetic payload.

It does not measure:

- socket receive latency;
- QUIC or TLS processing;
- leader routing;
- validator processing;
- transaction propagation;
- transaction landing;
- end-to-end submission latency.

## Target TPU Boundary

The intended future public transport boundary is:

1. accept an opaque signed serialized Solana transaction;
2. obtain current cluster routing information through a discovery provider;
3. select and maintain appropriate TPU ingress connections;
4. submit the transaction through supported TPU transport;
5. expose structured delivery status and telemetry.

That future transport functionality is not implemented by this prototype.

## Target Architecture Records

The target transaction-delivery architecture is specified separately from the current prototype:

- [Signed Transaction Delivery Boundary](docs/architecture/0003-transaction-delivery-boundary.md)
- [Delivery Status Semantics](docs/architecture/0004-delivery-status-semantics.md)
- [Discovery, Routing, Transport, and Observation Separation](docs/architecture/0005-topology-routing-transport-separation.md)

These records define architectural boundaries only. They do not change the implemented status described above.
