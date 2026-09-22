# 5. Separate Discovery, Routing, Transport, and Observation

Date: 2026-09-13

## Status

Accepted as the target architecture.

The current delivery library implements owned topology state, deterministic internal topology resolution, deterministic bounded route planning, monotonic topology-freshness evaluation, and callable local submission acceptance with request-owned transaction and target state. Discovery, adaptive routing, retries, connection management, transport attempts, transport, and observation described by this record remain unimplemented.

## Context

A native TPU delivery system has several concerns that change independently:

- cluster and leader information
- routing policy
- transport connection lifecycle
- transaction submission
- landing observation

Binding those responsibilities into one client object would make testing, alternative discovery sources, transport conformance, and failure injection unnecessarily difficult.

## Decision

The target architecture separates discovery, routing, transport, connection management, and observation behind explicit internal or public interfaces.

### Discovery provider

A discovery provider produces routing inputs without performing transaction transmission.

Its output may include:

- current slot context
- plausible upcoming leader identities
- validator contact information
- TPU transport endpoints
- freshness metadata

The initial implementation may use one discovery source, but the architecture must permit alternate providers without changing the transaction submission contract.

A Geyser implementation is not part of this library. A caller or separate component may provide topology through an adapter in the future.

### Topology snapshots

Routing should consume a coherent topology snapshot rather than repeatedly reading unrelated mutable fields during one route decision.

Snapshots should carry enough freshness information for callers and routing policy to detect stale topology.

### Validator identity and transport endpoint

Validator identity and network endpoint are separate concepts.

Routing metadata should retain validator identity, while reusable transport connections should be keyed by the endpoint identity required by the transport implementation.

The architecture must not assume that one validator identity always maps one-to-one to one unique socket address.

### Routing policy

Routing consumes topology and submission policy and produces an ordered set of target attempts.

The routing layer does not establish sockets and does not perform TLS or QUIC operations.

The architecture must support a plausible-leader frontier rather than requiring the caller-facing API to depend on perfect observation of one exact current leader.

Controlled fanout belongs to routing policy and must remain bounded and observable.

### Connection pool

Connection management owns reusable transport state independently of routing decisions.

Its responsibilities include:

- connection lookup
- connection establishment
- connection reuse
- bounded pool capacity
- idle and failure eviction
- prewarming policy hooks
- transport flow-control state

The pool does not decide which validators should receive a transaction.

### Transport

The transport layer receives a target endpoint and transaction bytes and reports transport-level progress or failure.

It does not:

- discover leaders
- choose routing fanout
- construct transactions
- claim transaction landing

The target architecture expects a mature QUIC/TLS implementation behind this boundary rather than a project-specific QUIC stack.

### Observation

Landing and confirmation observation are separate from delivery transport.

An observation provider may be absent. In that configuration the library can report transport evidence but must leave landing state unknown.

## Failure Isolation

The separation permits deterministic tests with synthetic discovery snapshots, route planners, transport doubles, and observation doubles without requiring a live cluster for every policy test.

It also permits black-box transport conformance tests against independent validator implementations without coupling those tests to a private trading engine.

## Consequences

The library can evolve discovery sources, routing algorithms, connection behavior, and observation mechanisms independently while preserving a stable caller-facing submission boundary.
