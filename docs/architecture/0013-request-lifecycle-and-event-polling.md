# 13. Request Lifecycle and Event Polling

Date: 2026-09-22

## Status

Accepted as the initial request-lifecycle and event-polling design.

The local-acceptance lifecycle, bounded event retention, and caller-driven polling behavior described by this record are implemented. Terminal request transitions, transport attempts, retries, transport, and observation remain unimplemented.

This record defines request-level event sequencing, bounded event retention, polling, lifecycle backpressure, and the relationship between request state and required events.

It does not define transport attempts, retry policy, QUIC or TLS behavior, landing observation, confirmation, or cancellation.

## Context

Callable submission now accepts an already-signed opaque transaction into library-owned request state.

A successful submission owns the transaction bytes, request identifier, topology generation, routing slot, and materialized route targets.

The public event envelope already separates request, transport-attempt, and observation classes and carries:

- a request identifier;
- an optional attempt identifier;
- a request-local sequence value;
- a monotonic timestamp;
- a diagnostic code;
- reserved extension space.

Only event codes with named public constants have assigned ABI semantics. `SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED` is currently the only concrete request event code; later codes remain undefined until corresponding behavior exists.

The next lifecycle boundary must make local acceptance observable without inventing transport evidence and must define what happens when callers do not drain events quickly enough.

## Request State

Request state remains internal implementation vocabulary.

The initial lifecycle distinguishes at least:

- locally accepted, nonterminal request state;
- terminal request state.

Transport-specific intermediate states may be added later without exposing an ABI-visible request-state object.

The public observation mechanism is the event stream rather than direct access to mutable internal request state.

The current implementation provides local acceptance observation and the lifecycle substrate required for later terminal transitions.

It does not fabricate a terminal transition for requests that have not entered a real transport lifecycle.

## Initial Request Event

Define the first concrete request event code:

```c
#define SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED UINT32_C(1)
```

This code is meaningful only when:

```text
event_class == SOLANA_DELIVERY_EVENT_CLASS_REQUEST
```

A successful callable submission produces exactly one accepted event.

That event contains:

- `struct_size` describing the event prefix written by the implementation;
- `event_class` equal to `SOLANA_DELIVERY_EVENT_CLASS_REQUEST`;
- `event_code` equal to `SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED`;
- `diagnostic_code` equal to zero;
- the nonzero accepted request identifier;
- `attempt_id` equal to `SOLANA_DELIVERY_ATTEMPT_ID_NONE`;
- `request_sequence` equal to one;
- a timestamp from the client monotonic clock domain;
- zero reserved fields.

No attempt event or observation event is created by local acceptance.

## Event Sequence

Every request owns a monotonically increasing event sequence.

The first observable event has sequence one.

Later events for the same request must use strictly increasing sequence values.

Sequence zero is not emitted.

Sequence values must not wrap and be reused.

If a future transition cannot allocate its next sequence value without wrapping, that transition must fail without mutating request state.

Request sequence establishes ordering only within one request.

No semantic ordering across different requests may be inferred from numeric sequence values or event timestamps.

## Submission Atomicity

Local request acceptance and retention of its accepted event form one logical commit.

Submission must not return `SOLANA_DELIVERY_STATUS_OK` unless both:

1. the request can enter library-owned state;
2. its accepted event can be retained for later polling.

If event retention capacity is unavailable, submission returns `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED`.

That failure:

- creates no request;
- consumes no request identifier;
- emits no event;
- leaves previously accepted requests unchanged.

If the monotonic timestamp required for the accepted event cannot be obtained, the clock failure status is propagated and the request is not accepted.

## Event Channel

The implementation owns a bounded event channel.

Its concrete storage representation and numeric capacity are private implementation details and are not public ABI.

The implementation must not silently overwrite an unread event.

The implementation must not silently discard an unread event.

A full event channel therefore creates explicit backpressure.

For callable submission, that backpressure appears as `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED` before local acceptance commits.

## Lifecycle Transition Atomicity

Any future lifecycle transition that requires an externally visible event must treat state mutation and event retention as one logical operation.

The transition may commit only if its required event can be retained.

If event retention capacity is unavailable:

- the required event is not discarded;
- the request does not advance to the new state;
- the previous request state remains authoritative;
- the transition reports resource pressure to its caller.

This rule applies to terminal transitions as well as nonterminal transitions.

A terminal state therefore cannot become externally invisible because the event channel was full.

Future transport scheduling may retry a blocked lifecycle transition after polling releases event capacity.

## Terminal Event Guarantee

Terminal event codes are not defined by this record because no concrete terminal transport behavior is implemented yet.

When terminal behavior is implemented, every terminal request transition must produce a corresponding request event under the lifecycle-transition atomicity rule.

A request must not be reclaimed before its required terminal event has been copied successfully into caller-owned poll storage.

This prevents request destruction from racing ahead of externally observable terminal state.

Until concrete terminal transitions exist, locally accepted requests remain owned by the client until client destruction.

## Polling API

The public API provides a nonblocking caller-driven event poll:

```c
solana_delivery_status_t solana_delivery_client_poll_events(
    solana_delivery_client_t *client,
    solana_delivery_event_t *events,
    size_t event_capacity,
    uint32_t event_stride,
    size_t *out_event_count
);
```

`out_event_count` is required.

When the output pointer itself is valid, the implementation sets `*out_event_count` to zero before attempting to consume events.

If `event_capacity` is zero:

- `events` may be null;
- no event is consumed;
- `*out_event_count` remains zero;
- the call returns `SOLANA_DELIVERY_STATUS_OK` when the remaining arguments are valid.

If `event_capacity` is greater than zero:

- `events` must be non-null;
- the event base pointer must satisfy the required alignment;
- `event_stride` must be large enough for the minimum supported event prefix;
- `event_stride` must preserve required element alignment;
- capacity and stride arithmetic must not overflow.

Polling copies at most `event_capacity` pending events.

Only events successfully copied to caller storage are consumed.

A call with fewer output slots than pending events succeeds with a partial drain.

Polling is nonblocking.

An empty event channel returns `SOLANA_DELIVERY_STATUS_OK` with an output count of zero.

## Event Stride and ABI Growth

The current event structure occupies 64 bytes.

That 64-byte layout becomes the minimum event prefix supported by the initial polling ABI.

Polling uses an explicit byte stride so event records may grow append-only in a later compatible ABI revision without forcing callers and libraries to assume identical `sizeof(solana_delivery_event_t)` values.

For each output slot, the implementation writes only the event prefix supported by both:

- the caller-provided stride;
- the implementation-known event structure.

The implementation does not write beyond the caller-provided stride.

The returned `struct_size` records the number of event-structure bytes written for that slot.

Bytes beyond that written prefix remain caller-owned and are not interpreted.

## Poll Consumption Order

The implementation retains events in deterministic queue order.

Polling removes events from the head of that queue and copies them into caller output slots in the same order.

This implementation ordering does not create a cross-request semantic ordering guarantee.

Within one request, `request_sequence` remains the authoritative ordering contract.

## Request Reclamation

A nonterminal request remains library-owned.

A terminal request may become reclaimable only after:

1. its terminal transition has committed;
2. its required terminal event has been copied by polling;
3. no other implementation-owned operation still references the request.

The concrete reclamation algorithm remains private.

Client destruction releases all request and event storage regardless of whether events were polled because destruction terminates the client ownership domain.

## Backpressure and Correctness

Event backpressure is correctness-visible.

The implementation prefers explicit resource exhaustion or deferred lifecycle transition over silent telemetry loss.

Required request-lifecycle events are part of the observable state machine, not best-effort logging.

Optional future diagnostics may use a weaker policy only if they are explicitly classified as non-semantic telemetry.

## Threading

Phase 1 continues to require exclusive access to one client for:

- topology installation;
- submission;
- event polling;
- destruction.

This record does not introduce concurrent submission, concurrent polling, or concurrent topology replacement guarantees.

Future concurrency support requires an explicit synchronization contract and dedicated tests.

## Excluded Behavior

This lifecycle boundary does not:

- open network connections;
- create transport attempts;
- allocate attempt identifiers;
- emit attempt events;
- implement QUIC or TLS;
- retry delivery;
- cancel requests;
- infer validator receipt;
- infer transaction landing;
- infer confirmation;
- implement observation events.

`SOLANA_DELIVERY_ATTEMPT_ID_NONE` therefore remains the only attempt identifier used by events produced in this phase.

## Consequences

Callable submission becomes observably asynchronous without pretending that local acceptance is transport progress.

The first concrete event code is derived from implemented local acceptance behavior rather than speculative transport semantics.

The bounded event channel establishes an explicit pressure boundary.

State-and-event atomicity prevents required lifecycle transitions from becoming invisible when that channel is full.

Explicit event stride preserves append-only event ABI growth.

Future transport work can add attempt lifecycle behavior on top of request sequencing and polling without redefining local acceptance semantics.
