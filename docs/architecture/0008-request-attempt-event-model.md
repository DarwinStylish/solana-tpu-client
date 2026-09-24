# 8. Request and Attempt Event Model

Date: 2026-09-13

## Status

Accepted as the Phase 1 event design.

## Context

One transaction request may be routed to several candidate leaders and may be retried. The caller needs enough information to understand what the delivery library actually did without conflating local acceptance, transport progress, and on-chain observation.

## Decision

Events are correlated through a request identifier and, where applicable, an attempt identifier.

The event model has three semantic layers:

1. request lifecycle
2. transport-attempt lifecycle
3. external observation lifecycle

## Request Events

Request-level events may report facts such as:

- locally accepted
- route planned
- retry or fanout policy exhausted
- completed without landing knowledge
- cancelled when cancellation becomes supported

Request completion does not inherently mean the transaction landed.

## Attempt Events

Attempt-level events may report facts such as:

- target selected
- waiting for connection capacity
- connection or protocol failure
- transport operation started
- transport bytes accepted by the underlying transport implementation
- transport operation completed
- transport operation failed

The exact numeric event set must be derived from implemented transport behavior before the ABI is frozen.

## Observation Events

Observation events are emitted only when an observation mechanism provides the corresponding evidence.

They may eventually represent facts such as landed, confirmed, expired, or observation timeout according to explicitly defined semantics.

No transport event may be relabeled as an observation event.

## Event Ordering

Events for one request carry a monotonically increasing library-local sequence value.

The sequence is for event ordering within that request. It is not a validator sequence, transaction sequence, slot, or network timestamp.

Cross-request ordering must not be inferred unless separately documented.

## Timestamps

Telemetry timestamps must identify their clock domain.

Library-generated duration and ordering measurements should use a monotonic clock.

External chain or validator timestamps, if introduced later, must be represented separately and labeled by source rather than being written into a generic timestamp field.

## Event Storage

Polling copies events into caller-provided storage.

Variable-size diagnostic strings are not required in the stable hot-path event structure.

Diagnostic codes and bounded numeric fields are preferred. Optional diagnostic text may be retrieved through a separate non-hot-path interface if needed.

## Backpressure

The event channel itself is a bounded resource.

The implementation must define what occurs if the caller fails to drain events fast enough.

It must not silently overwrite or discard semantically required terminal request events.

Phase 1 implementation must choose and test a bounded policy before the event ABI is declared stable.

## Consequences

Fanout and retries remain visible as multiple attempts under one submission request.

Callers can build telemetry and observability without interpreting transport progress as transaction landing.
