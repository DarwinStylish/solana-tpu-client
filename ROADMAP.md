# Technical Roadmap

This roadmap describes engineering phases for the native Solana transaction-delivery library.

It does not change the current implementation status in `CURRENT_STATE.md`.

## Scope

The target system accepts opaque already-signed serialized transactions and attempts delivery to appropriate Solana TPU ingress endpoints through a standalone native library.

The roadmap deliberately excludes wallet functionality, transaction construction, signing, private trading strategy, validator implementation, hosted relay services, Geyser implementation, custom QUIC development, and kernel-bypass networking.

## Phase 1 — Submission API and Topology Boundary

Define and implement the public transaction-delivery API without implementing production TPU transport.

Current progress includes the delivery ABI vocabulary, extensible topology-array layout, pure topology structural validation, topology fuzz coverage, opaque client lifecycle, transactional copy-on-install topology ownership, strict snapshot-generation ordering, local monotonic receipt-time tracking, deterministic internal slot-to-topology-candidate resolution, and deterministic bounded route planning over resolved candidates.

Expected work:

- versioned C ABI for opaque signed transaction submission
- explicit caller-buffer ownership and lifetime contract
- request identifiers
- submission options and bounded policy inputs
- library-defined result and error model
- discovery-provider interface
- immutable topology snapshot representation
- validator identity separated from transport endpoint identity
- topology freshness and submission-policy semantics
- deterministic route-planner tests using synthetic topology

Exit criteria:

- public headers compile as C11 and C++
- no private execution-core dependency
- API tests pin ABI-sensitive layout intentionally
- topology and submission policy can be tested without a live cluster
- current inbound decoder remains clearly separate from the delivery API

## Phase 2 — TPU Transport and Connection Lifecycle

Implement production transport behind the transport abstraction.

Expected work:

- mature-library-backed QUIC and TLS integration
- required TPU protocol negotiation
- validator identity handling required by the transport contract
- stake-weighted QoS support where applicable to the configured identity
- endpoint-keyed connection pool
- bounded pool capacity
- connection reuse and prewarming
- idle and failure eviction
- stream and transport flow-control handling
- graceful shutdown and cleanup

Exit criteria:

- transport can be tested independently from routing
- connection lifecycle has deterministic failure tests
- transport status never claims transaction landing
- no custom QUIC protocol implementation is introduced

## Phase 3 — Routing, Resilience, and Telemetry

Add bounded adaptive routing over the transport layer.

Expected work:

- plausible-leader-frontier construction
- ordered target selection
- bounded controlled fanout
- connection prewarm integration
- explicit retry policy
- backpressure handling
- retry exhaustion semantics
- per-request telemetry
- per-attempt telemetry
- latency and failure counters
- synthetic failure injection

Exit criteria:

- every transport attempt is attributable to one request and target
- retry and fanout decisions are observable
- queue or transport pressure cannot silently discard a request
- aggregate request status preserves attempt-level evidence
- routing policy can be tested without real network transport

## Phase 4 — Ingress Interoperability and Conformance

Validate the delivery implementation as a black-box client against independent validator ingress implementations.

Expected work:

- reproducible local or controlled test environments
- Agave TPU-ingress interoperability tests
- Firedancer or Frankendancer TPU-ingress interoperability tests
- malformed and boundary-input tests
- disconnect and reconnect scenarios
- flow-control stress scenarios
- sanitizer coverage
- fuzz coverage for pure parsing and policy boundaries
- benchmark methodology for transport and routing paths
- reproducible conformance reports

Exit criteria:

- conformance tests distinguish transport evidence from landing evidence
- failures identify the layer at which interoperability broke
- benchmark reports describe the measured path and environment
- release artifacts build from a clean tracked checkout

## Deferred Work

The following remain outside the initial delivery-library scope:

- wallet and signer implementation
- transaction construction SDK
- trading strategy
- portfolio and position management
- Jito bundle submission
- hosted relay infrastructure
- validator implementation
- Geyser implementation
- custom QUIC stack
- DPDK or other kernel-bypass networking

Those capabilities may be integrated by callers or separate projects without expanding the core delivery-library boundary.
