# 11. Submission Admission and Topology Freshness

Date: 2026-09-22

## Status

Accepted as the initial internal submission-admission policy.

This record defines the policy gate that determines whether an installed topology snapshot is fresh enough to proceed toward topology resolution and bounded route planning.

It does not define a callable public submission API, transaction ownership, request lifecycle, transport attempts, retries, or event emission.

## Context

The delivery client already owns an installed topology snapshot and records the local monotonic time at which that snapshot was accepted.

Topology generation, caller-observed chain slot context, and local monotonic snapshot age are separate concepts.

Generation orders topology replacements within one client lifetime. It is not a time source.

`current_slot` records caller-observed chain context. It is not a monotonic clock and does not define snapshot age.

Submission therefore needs an explicit admission stage before resolution and route planning so stale topology is rejected without embedding freshness behavior into either topology interpretation or target selection.

## Decision

Introduce an internal submission policy containing:

- a maximum permitted topology age in monotonic nanoseconds;
- a positive route target limit.

The initial internal representation is:

```c
typedef struct {
    uint64_t max_topology_age_ns;
    size_t target_limit;
} solana_delivery_submission_policy_t;
```

This representation is internal implementation vocabulary and is not part of the public C ABI.

## Admission Order

Submission admission evaluates policy before topology resolution or route planning.

The initial order is:

1. validate the submission policy;
2. require an installed topology snapshot;
3. sample the client monotonic clock exactly once;
4. determine the installed snapshot age;
5. reject topology whose age exceeds policy;
6. permit later resolution and bounded planning to proceed.

Admission success means only that policy permits later routing work to continue.

It does not mean that the snapshot contains a usable leader, endpoint, or route.

## Policy Validation

`max_topology_age_ns` must be greater than zero.

A zero maximum age does not mean unlimited freshness and does not disable freshness checking.

`target_limit` must also be greater than zero.

A future API may introduce explicit opt-out or alternate policy modes if required. The initial policy does not encode such behavior through sentinel zero values.

## Monotonic Freshness

Freshness is evaluated only from:

- the local monotonic receipt time recorded when the installed snapshot was accepted;
- one new sample from the same client monotonic clock domain.

If the new monotonic sample is earlier than the stored receipt time, admission returns `SOLANA_DELIVERY_STATUS_INTERNAL_ERROR` rather than performing unsigned subtraction.

Otherwise:

```text
age = now_monotonic_ns - received_monotonic_ns
```

The snapshot is stale only when:

```text
age > max_topology_age_ns
```

An age exactly equal to `max_topology_age_ns` remains admissible.

A stale installed snapshot returns `SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE`.

## Clock Failure

If the configured monotonic clock source cannot provide the admission sample, its non-success status is propagated.

Admission does not mutate client state, so a clock failure cannot partially change the installed topology or submission state.

## Topology Availability

A client with no installed topology returns `SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE` before any freshness calculation.

Fresh topology may still later return `SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE` during resolution when no requested routing context produces candidates.

Freshness and route availability are therefore separate decisions.

## Generation Semantics

Topology generation is not used in freshness arithmetic.

Generation remains solely an ordering mechanism for topology replacement.

A high generation does not imply a fresh snapshot, and a low generation does not imply an old snapshot.

## Slot Semantics

`current_slot` is not used to calculate monotonic snapshot age.

This admission policy does not compare a requested routing slot with `current_slot`.

It does not assume a Solana slot duration and does not convert slot distance into elapsed time.

Requested-slot selection, plausible-leader-frontier construction, and bounded future-slot routing remain separate routing-policy concerns.

## Target Limit

The positive target limit is carried by submission policy because it bounds the route plan associated with one eventual submission request.

Admission itself does not create targets.

After successful freshness admission and topology resolution, the target limit may be passed unchanged to the deterministic route planner.

Output buffer capacity remains separate from target policy. A target limit constrains planning semantics; caller-provided capacity constrains storage.

## Purity and State

Submission admission:

- performs no allocation;
- performs no network activity;
- does not modify the installed topology;
- does not modify generation;
- does not modify receipt time;
- does not create a request identifier;
- does not create an attempt identifier;
- does not emit events.

Its only external dependency is the client monotonic clock source.

## Excluded Behavior

This policy does not:

- select a requested slot;
- construct a plausible-leader frontier;
- resolve topology candidates;
- deduplicate or rank route targets;
- allocate transaction storage;
- accept transaction bytes;
- create request lifecycle state;
- perform retries;
- inspect connection state;
- perform QUIC or TLS operations;
- claim transaction landing or confirmation.

Those behaviors remain later submission, routing, transport, and observation work.

## Public ABI

The initial admission policy remains internal.

This decision does not modify `include/solana/delivery.h` and does not yet assign additional public fields to `solana_delivery_submit_options_t`.

The public submission-options vocabulary can be mapped to an internal policy when callable submission semantics are implemented and tested.

## Consequences

Topology freshness becomes independently testable with an injected monotonic clock.

Resolution remains a deterministic interpretation layer rather than acquiring hidden time dependence.

Route planning remains a pure candidate-to-target transformation rather than acquiring topology-age checks.

Future submission composition can therefore follow an explicit sequence:

```text
admission -> resolution -> bounded route planning -> request lifecycle
```

without conflating freshness, topology interpretation, routing policy, or transport.
