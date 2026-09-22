# 10. Deterministic Bounded Route Planning

Date: 2026-09-22

## Status

Accepted as the initial internal route-planner policy.

This record defines deterministic policy over already-resolved topology candidates. It does not define a public routing ABI, transport attempts, retries, or adaptive routing.

## Context

Topology resolution now converts one installed topology snapshot and requested slot into a deterministic candidate sequence.

That resolver intentionally preserves repeated relationships and performs no ranking, deduplication, fanout selection, freshness policy, retry behavior, allocation, or network activity.

A separate route-planning stage is required so policy can evolve without changing topology interpretation.

## Decision

Introduce an internal pure route planner that consumes an already-resolved candidate sequence.

The planner does not inspect topology arrays directly and does not invoke discovery, connection management, or transport.

Its initial policy is:

1. consume candidates in resolver-provided order;
2. treat `(validator_index, endpoint_index)` as the target identity used for duplicate suppression;
3. preserve only the first occurrence of each target identity;
4. retain the `leader_index` from that first occurrence as planning provenance;
5. preserve first-occurrence order among unique targets;
6. select at most the configured positive target limit.

This is deterministic prefix selection, not adaptive target ranking.

## Target Identity

Duplicate suppression uses the pair:

- validator index;
- endpoint index.

Endpoint index alone is not sufficient because multiple validators may legitimately reference the same transport endpoint.

Validator index alone is not sufficient because one validator may legitimately expose multiple endpoints.

The planner therefore preserves the architectural distinction between validator identity and transport endpoint identity.

## Leader Provenance

The target representation retains a leader index in addition to validator and endpoint indices.

When multiple candidates resolve to the same validator-and-endpoint pair, the first candidate wins and its leader index is retained.

Later telemetry may use that provenance to explain why the target entered a route plan. The leader index is not part of target deduplication identity.

## Ordering

The planner performs no reordering.

After duplicate suppression, target order is the order of first occurrence in the resolver candidate sequence.

The initial policy performs no:

- stake ranking;
- latency ranking;
- endpoint preference;
- randomization;
- connection-state ranking;
- adaptive scoring.

Given the same candidate sequence and target limit, the plan is deterministic.

## Bounded Selection

The target limit must be greater than zero.

After duplicate suppression, the planner selects the first `min(unique_target_count, target_limit)` targets.

The planner reports both:

- the complete number of unique targets represented by the candidate sequence;
- the number selected after applying the target limit.

This makes policy truncation explicit even before request telemetry exists.

A target limit constrains the route plan only. It does not itself create transport attempts or establish concurrent fanout.

## Output Storage

The planner uses caller-provided target storage and performs no allocation.

If output capacity is smaller than the selected target count:

- no partial target sequence is written;
- the selected target count is reported;
- the complete unique target count is reported;
- the operation returns `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED`.

A zero-capacity output call may therefore be used to determine the required selected-target capacity.

## Availability

An empty candidate sequence has no route to plan and returns `SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE`.

The planner does not independently decide whether an installed topology is stale, whether a requested slot is appropriate, or whether an endpoint is currently reachable.

Those facts must be established by their owning layers.

## Validation Boundary

A nonzero candidate count requires a non-null candidate array.

A positive output capacity requires a non-null target array.

A zero target limit is invalid because it does not describe a usable bounded routing policy.

Candidate indices are trusted internal output from topology resolution. The planner does not repeat topology structural validation.

## Excluded Policy

This initial planner does not:

- evaluate topology freshness;
- construct future-slot or multi-slot plausible-leader frontiers;
- rank targets;
- inspect stake;
- inspect connection state;
- schedule retries;
- create request or attempt identifiers;
- allocate or manage connections;
- perform QUIC or TLS operations;
- submit transaction bytes;
- emit delivery events;
- claim transaction landing.

Plausible-leader-frontier expansion, adaptive ordering, retries, and transport-aware policy remain later routing work.

## Public ABI

Route-planner types and functions remain internal.

This decision does not modify `include/solana/delivery.h` and does not freeze a public routing-policy ABI.

## Consequences

Topology interpretation and routing policy remain independently testable.

Synthetic tests can pin duplicate suppression, first-occurrence ordering, leader provenance, target-limit truncation, capacity atomicity, and shared-endpoint behavior without live network transport.

Later routing policies may replace deterministic prefix selection without changing the topology resolver or prematurely committing those policies to the public ABI.
