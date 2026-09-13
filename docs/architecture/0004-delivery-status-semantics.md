# 4. Delivery Status Semantics

Date: 2026-09-13

## Status

Accepted as the target architecture.

The functionality described by this record is not implemented by the current prototype.

## Context

Transaction delivery crosses several independent boundaries. A caller request may be accepted locally, routed to multiple targets, written through one or more transports, and later observed on-chain.

Collapsing those events into a single success flag would make the API ambiguous and could incorrectly equate transport progress with transaction execution.

## Decision

The future library will distinguish request state, transport-attempt state, and external observation state.

### Request identity

Each accepted submission receives a library-level request identity suitable for correlating routing decisions, transport attempts, retries, and observations.

A single request may create multiple target attempts.

### Transport attempts

Each target attempt is independently observable.

Transport telemetry may describe facts such as:

- target selection
- connection availability
- connection establishment failure
- transport backpressure
- stream or send attempt creation
- byte-write completion
- transport-level completion or failure
- retry scheduling

The exact status enumeration belongs to the future public ABI and must be derived from implemented transport behavior.

### Landing and confirmation

Transport completion is not transaction landing.

A successful connection, successful stream operation, successful byte write, or other transport-level acknowledgement must not be reported as proof that the transaction was accepted into a block.

Landing or confirmation requires evidence from an observation mechanism that is explicitly responsible for that determination.

The observation mechanism remains logically separate from the transport layer.

### Aggregate status

When routing policy creates multiple attempts, request-level reporting must preserve the underlying attempt information.

An aggregate status must never erase which targets were attempted or convert partial transport success into an unsupported landing claim.

### Failure taxonomy

The implementation should preserve failure origin where practical, including distinctions among:

- local input rejection
- unavailable topology
- no eligible route
- connection failure
- handshake or protocol failure
- backpressure
- transport write failure
- retry exhaustion
- observation timeout or expiry

The public ABI should use library-defined status values rather than exposing platform-specific error codes as the sole semantic contract.

Platform error information may still be attached as diagnostic telemetry.

## Consequences

Callers can reason independently about whether a request was accepted by the library, whether bytes progressed through a transport, and whether the transaction was later observed as landed or confirmed.

This separation also makes black-box interoperability testing possible without inventing stronger success semantics than the observed system provides.
