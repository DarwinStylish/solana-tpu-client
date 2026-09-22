# 9. Deterministic Topology Resolution

Date: 2026-09-13

## Status

Accepted as the initial internal topology-resolution design.

This record defines deterministic interpretation of an installed topology snapshot. It does not define public routing policy or a public routing ABI.

## Context

The delivery client now owns a validated topology snapshot containing validators, transport endpoints, validator-to-endpoint associations, and leader slot ranges.

Routing policy needs a deterministic way to turn that owned representation into candidate validator and endpoint references without performing discovery, ranking, network I/O, retry policy, or connection management.

Keeping this step separate prevents basic topology interpretation from being conflated with higher-level routing decisions.

## Decision

Introduce an internal topology-resolution operation that consumes only the client-owned snapshot and a requested slot.

Resolution proceeds in stored topology order:

1. examine leader records in array order;
2. select each leader whose inclusive slot range contains the requested slot;
3. for each selected leader, examine validator-to-endpoint associations in array order;
4. select each association whose validator index matches that leader;
5. emit one candidate containing the leader, validator, and endpoint indices.

The initial candidate representation is internal implementation state and is not part of the stable public C ABI.

## Candidate Identity

A resolved candidate preserves three separate references:

- leader record index;
- validator record index;
- endpoint record index.

Validator identity and transport endpoint identity remain separate concepts.

Resolution must not collapse them into one implicit target identity.

## Deterministic Ordering

Candidate order is derived only from the installed snapshot:

- leader array order is primary;
- association array order is secondary.

Resolution performs no latency ranking, stake ranking, randomization, endpoint preference, or adaptive reordering.

Given the same installed topology and requested slot, the result is deterministic.

## Duplicate Semantics

Topology resolution does not deduplicate candidates.

Overlapping leader records, repeated validator-to-endpoint associations, or other structurally valid repeated relationships remain visible in the resolved sequence.

Duplicate suppression, target ranking, bounded fanout, and retry behavior belong to routing policy and are defined separately.

## Availability

Resolution returns `SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE` when:

- the client has no installed topology;
- no leader range contains the requested slot; or
- matching leaders produce no validator-to-endpoint candidates.

An installed but empty topology therefore remains valid state while being unavailable for resolution.

## Buffer Semantics

The internal resolver uses caller-provided candidate storage rather than allocating candidate arrays.

Resolution first determines the complete candidate count.

If the supplied capacity is insufficient:

- no partial candidate sequence is returned;
- the required candidate count is reported;
- the operation returns `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED`.

A zero-capacity call may therefore be used to determine required capacity.

Candidate-count arithmetic must detect `size_t` overflow rather than wrap.

## Validation Boundary

Resolution operates only on topology already accepted and internalized by the client.

It does not repeat structural validation and does not attempt to recover from corrupted internal state.

Caller-supplied topology remains subject to `solana_delivery_topology_validate` and transactional installation before resolution.

## Excluded Policy

This resolution layer does not:

- evaluate topology age;
- compare the requested slot with `current_slot` as a freshness rule;
- construct a plausible-leader frontier beyond literal matching ranges;
- rank or deduplicate candidates;
- choose fanout width;
- perform retries;
- allocate or manage connections;
- perform QUIC or TLS operations;
- submit transactions;
- report landing or confirmation.

Those behaviors belong to later routing, transport, and observation layers.

## Consequences

Synthetic topology tests can establish exact slot-to-candidate behavior before routing policy exists.

The route planner can later consume a deterministic candidate sequence while remaining responsible for policy decisions such as deduplication, ranking, fanout, and retry.

No new public ABI commitment is required by this step.
