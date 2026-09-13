# Current Implementation Status

## Purpose

The repository name reflects the intended Solana TPU transaction-delivery project. The current codebase is a standalone pre-transport ingress prototype and must not be interpreted as a complete TPU client.

## Implemented

The current revision implements:

- a public C11 prototype API;
- a fixed-layout little-endian trade-event decoder;
- bounds and side-discriminator validation;
- a loopback-only non-blocking UDP receiver;
- callback-based event delivery;
- callback-driven lifecycle coordination with synchronization owned by the caller;
- localhost integration testing;
- a synthetic decoder microbenchmark;
- a standalone static-library build.

The public repository no longer requires private HFT engine headers or types to compile.

## Public/Private Boundary

The public prototype exposes schema-local decoded fields only.

It does not expose or depend on:

- private execution-event representations;
- private fixed-point types;
- private venue identifiers;
- private SPSC queues;
- trading strategy state;
- execution-engine state.

Translation from the public API into any private execution model belongs to the consuming application.

## Current API Contract

The fixed-layout decoder accepts exactly 33 bytes. It rejects null pointers, truncated inputs, oversized inputs, and unsupported side discriminators.

The instruction discriminator is currently exposed as an opaque schema field. This prototype does not define an authoritative set of instruction values and therefore does not reject values based on instruction semantics.

`solana_ingress_event_t` currently occupies 48 bytes. Its reserved bytes are zero-initialized by the decoder and must not be interpreted by consumers. Tests pin the current size and field offsets so accidental ABI drift is detected.

Event callbacks are synchronous. The event pointer is valid only while the callback is executing. Event and lifecycle contexts are caller-owned.

The local receiver returns zero after caller-requested shutdown. Operational failures return `-1`; the originating `errno` value is preserved across socket cleanup.

The API is pre-release and is not yet declared a permanent version-1 ABI.

## Not Implemented

The current revision does not implement:

- Solana TPU transaction submission;
- QUIC client transport;
- TLS identity handling;
- `solana-tpu` ALPN negotiation;
- leader schedule tracking;
- TPU contact-information resolution;
- connection pooling or prewarming;
- stake-weighted QoS behavior;
- transaction routing or leader fanout;
- retry and backpressure policies;
- landing or confirmation observation;
- Agave/Firedancer TPU-ingress conformance;
- kernel-bypass networking.

## Prototype Parser Scope

The current decoder operates on one project-specific 33-byte little-endian trade-event layout.

It is not:

- a Solana transaction decoder;
- a general Borsh implementation;
- the Solana TPU wire protocol.

The receiver maintains a local datagram sequence counter. Successfully decoded events carry the counter value assigned when their datagram was received; rejected datagrams may therefore create gaps. The value is not validator- or exchange-assigned.

## Benchmark Scope

The current benchmark repeatedly decodes one preallocated synthetic payload.

It does not measure:

- socket receive latency;
- QUIC or TLS processing;
- leader routing;
- validator processing;
- transaction propagation;
- transaction landing;
- end-to-end submission latency.

## Target TPU Boundary

The intended future public transport boundary is:

1. accept an opaque signed serialized Solana transaction;
2. obtain current cluster routing information through a discovery provider;
3. select and maintain appropriate TPU ingress connections;
4. submit the transaction through supported TPU transport;
5. expose structured delivery status and telemetry.

That future transport functionality is not implemented by this prototype.

## Target Architecture Records

The target transaction-delivery architecture is specified separately from the current prototype:

- [Signed Transaction Delivery Boundary](docs/architecture/0003-transaction-delivery-boundary.md)
- [Delivery Status Semantics](docs/architecture/0004-delivery-status-semantics.md)
- [Discovery, Routing, Transport, and Observation Separation](docs/architecture/0005-topology-routing-transport-separation.md)

These records define architectural boundaries only. They do not change the implemented status described above.
