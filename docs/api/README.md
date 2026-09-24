# Public API Contracts

The public C interface is defined by the headers under `include/solana/`.
Those headers are the canonical function, ownership, lifetime, and ABI contracts.

This document summarizes behavior that spans more than one public header.

## Interfaces

| Header | Contract |
| --- | --- |
| `solana/delivery.h` | Topology, client lifecycle, submission, status, and delivery events |
| `solana/discovery.h` | Caller-owned discovery providers and explicit topology refresh |
| `solana/ingress.h` | Fixed-layout local ingress prototype |

## ABI Versioning

`SOLANA_DELIVERY_ABI_VERSION` currently identifies delivery ABI schema 1.0.
It is an ABI identifier, not the project release version. The project remains pre-release
and has not published a versioned release.

Documented extensible records begin with `struct_size`. Compatible ABI revisions may append
fields without changing the position or meaning of existing fields within one ABI major.

Topology arrays additionally carry explicit byte strides. A consumer therefore does not
assume that its own `sizeof(element_type)` is the stride of caller-provided records.

Reserved fields must contain the documented neutral value, currently zero.

## Client Ownership and Concurrency

`solana_delivery_client_create` creates an opaque library-owned client handle.
`solana_delivery_client_destroy` releases all implementation-owned state and accepts NULL.

Concurrent operations on one delivery client are not part of the public contract.
The caller serializes topology installation, topology refresh, submission, event polling,
and destruction for a given client.

No public structure exposes internal allocator, socket, QUIC, TLS, thread, or queue objects.

## Topology Ownership

Caller topology records and arrays remain caller-owned.

`solana_delivery_topology_validate` borrows them only for validation and retains nothing.

`solana_delivery_client_install_topology` validates and copies the ABI prefixes understood
by the implementation before returning success. After success the caller may reuse or
release its topology storage.

Topology replacement is failure-atomic. A rejected or incomplete replacement leaves the
previously installed topology authoritative.

## Discovery Provider Lifetime

A discovery provider descriptor and its context remain caller-owned.

Refresh is explicit and synchronous. A successful provider acquisition establishes a
borrowed topology snapshot for that refresh operation. The client passes it through normal
topology installation and invokes the optional release callback exactly once after the
installation attempt.

The client does not retain the provider, provider context, snapshot pointer, or provider
arrays after refresh returns.

Submission does not perform implicit discovery.

## Submission Ownership

`solana_delivery_client_submit` accepts an already-signed opaque serialized transaction.
The delivery layer does not construct, modify, sign, or reinterpret that transaction.

The caller retains ownership of the input byte range. A successful submission copies the
bytes and materializes the selected target information required by the accepted request.
No borrowed transaction pointer survives the call.

A successful submission assigns a nonzero request identifier unique within the client
lifetime and retains exactly one accepted request event.

Submission failure is atomic from the caller perspective: no request is accepted and a
valid output request identifier remains `SOLANA_DELIVERY_REQUEST_ID_NONE`.

## Status Semantics

Delivery status values describe immediate library-operation results.

- `SOLANA_DELIVERY_STATUS_OK` means the requested local operation succeeded.
- `SOLANA_DELIVERY_STATUS_INVALID_ARGUMENT` reports malformed or invalid API input.
- `SOLANA_DELIVERY_STATUS_UNSUPPORTED` reports a represented value or option not supported
  by the implementation.
- `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED` reports bounded capacity, allocation, or
  identifier exhaustion where applicable.
- `SOLANA_DELIVERY_STATUS_TOPOLOGY_UNAVAILABLE` reports missing topology or route state
  required by an operation.
- `SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE` reports topology rejected by generation or
  freshness requirements.
- `SOLANA_DELIVERY_STATUS_INTERNAL_ERROR` reports an internal, provider, or clock failure
  that cannot be represented as ordinary caller rejection.

The exact statuses applicable to each function are determined by that function contract.

Submission success means local request acceptance only. It does not mean that a connection
exists, bytes were transmitted, a validator received the transaction, or the transaction
landed or was confirmed.

## Event Polling

`solana_delivery_client_poll_events` is nonblocking.

Events are copied into caller-provided storage. Only events successfully copied to caller
storage are consumed. Partial drains are supported.

When capacity is zero, the event pointer may be NULL and no event is consumed.

Event codes are scoped by event class. Only explicitly named event-code constants have
assigned public semantics. The current implementation defines
`SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED` for request events.

An accepted request event has no transport attempt and therefore carries
`SOLANA_DELIVERY_ATTEMPT_ID_NONE`.

## Ingress Prototype

The ingress interface is separate from transaction delivery.

`solana_ingress_decode` accepts exactly one fixed-layout 33-byte prototype event.
Callback event storage supplied by the local receiver is valid only during the synchronous
callback. Callers that retain an event copy it before the callback returns.

The ingress record is not a Solana transaction representation and is not the TPU wire
protocol.

## Examples

- [Local delivery acceptance](../../examples/delivery_local_acceptance.c)
- [Explicit discovery refresh](../../examples/discovery_refresh.c)
- [Ingress decode](../../examples/ingress_decode.c)
- [Example build instructions](../../examples/README.md)
