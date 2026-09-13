# 6. Stable C ABI for Transaction Delivery

Date: 2026-09-13

## Status

Accepted as the Phase 1 API design.

The opaque client lifecycle and caller-supplied topology installation portions are implemented. Transaction submission, event polling, routing, transport, and observation behavior described by this record remain unimplemented.

## Context

The transaction-delivery architecture requires a public interface that can be consumed from C, C++, Rust, and other systems languages without importing application-specific runtime or private execution-engine types.

The existing `solana/ingress.h` interface belongs to the inbound decoder prototype and does not define the delivery ABI.

## Decision

Define the public delivery API under `solana/delivery.h`.

The initial delivery ABI will use:

- C11-compatible declarations
- fixed-width integer types for ABI-visible scalar values
- opaque handles for implementation-owned state
- explicit byte pointer plus length pairs
- structure-size fields for extensible public structures
- library-defined fixed-width status codes rather than C enums as ABI storage
- reserved fields for forward-compatible extension where appropriate
- explicit ownership and lifetime contracts
- no private execution-engine types

## ABI Versioning

The delivery header will expose a delivery ABI version constant.

Public extensible structures will begin with a `struct_size` field.

A caller sets `struct_size` to the size of the structure it compiled against. Implementations must reject structures smaller than the minimum required prefix and must not read fields beyond the supplied size.

New fields may be appended in later compatible revisions. Existing fields must not be reordered or silently change meaning within one ABI major version.

C enum object layout will not be used as a stable ABI storage contract. Public status and discriminator values will use explicitly sized integer typedefs with named constants.

## Client Lifetime

The delivery implementation will expose an opaque client handle.

Client creation owns implementation state. Client destruction releases implementation-owned resources.

No public structure may expose internal QUIC, TLS, socket, thread, allocator, or runtime objects.

## Submission Buffer Ownership

A submission call receives:

- a pointer to already-signed serialized transaction bytes
- an explicit byte length
- submission options
- storage for the resulting request identifier

The caller retains ownership of its input buffer.

The implementation must not retain a borrowed pointer after the submission call returns.

If accepted work must outlive the call, the implementation must copy or otherwise internalize the required bytes before returning success.

This rule deliberately favors an unambiguous ABI lifetime contract over implicit zero-copy behavior.

A future explicitly versioned ownership-transfer or retained-buffer API may be introduced separately if measurement justifies it.

## Submission Result Semantics

The direct return value from submission reports only whether the library accepted the request into its local delivery lifecycle.

It does not report:

- transport success
- validator acceptance
- transaction landing
- transaction confirmation

Local rejection reasons may include invalid arguments, unsupported options, resource exhaustion, unavailable required state, or other library-defined API failures.

Runtime delivery progress is reported separately through delivery events.

## Request Identity

Every locally accepted submission receives a library-generated request identifier.

The identifier is opaque to callers. Callers may compare and store it but must not infer routing, slot, validator, or transport semantics from its bits.

The initial implementation may use a fixed-width value sufficient to guarantee uniqueness within one client lifetime.

Request identifiers are correlation identifiers, not Solana transaction signatures.

## Attempt Identity

One request may generate zero, one, or multiple transport attempts.

Each attempt receives an identifier unique within its request.

Attempt identifiers exist so retry and fanout behavior remain observable without pretending that each transport attempt is a separate transaction submission.

## Event Retrieval

The initial delivery ABI will use caller-driven event polling rather than mandatory user callbacks.

Polling provides:

- explicit caller thread ownership
- no hidden callback reentrancy contract
- straightforward FFI integration
- deterministic event-storage lifetime
- simple integration with external event loops

A poll operation copies one or more pending library events into caller-provided storage.

Returned event structures remain caller-owned after the poll call because their contents were copied into caller storage.

The existence of the polling API does not prohibit an implementation from using internal worker threads.

A future notification mechanism may wake callers when events become available, but notification is separate from event ownership.

## Threading

The public contract must document which functions may be called concurrently on one client.

Phase 1 must not imply thread safety merely because the underlying implementation uses atomics or internal synchronization.

At minimum, creation and destruction require exclusive ownership of the handle.

Phase 1 topology installation also requires exclusive ownership of the handle. No concurrent installation, submission, polling, or destruction semantics are implied until they are explicitly implemented and tested.

Concurrent submission or polling support must be explicitly tested before being declared part of the contract.

## Error Model

Public API functions return library-defined status values.

Platform-specific error values may be exposed as optional diagnostics but are not the sole public semantic contract.

The library must distinguish API rejection from asynchronous request and attempt outcomes.

## Consequences

The initial ABI may perform a bounded transaction-byte copy on accepted submission.

That cost is measurable and can later be optimized through an additional explicit ownership model without weakening the original lifetime guarantee.

The ABI remains independent of the implementation language used behind the C boundary.
