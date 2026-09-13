# Solana TPU Client

Status: standalone pre-transport prototype.

This repository is intended to evolve into a native Solana TPU transaction-delivery library. The current implementation does not submit transactions to validator TPU endpoints and does not implement Solana TPU QUIC transport.

## Current Implementation

The repository currently provides a standalone experimental ingress module:

- a public C11 header under `include/solana/`;
- a fixed-layout little-endian trade-event decoder;
- a loopback-only non-blocking UDP ingress prototype;
- callback-based delivery with no dependency on a private execution engine;
- a localhost integration test;
- a synthetic decoder microbenchmark;
- a static library build artifact.

See [CURRENT_STATE.md](CURRENT_STATE.md) for the exact implementation boundary.

## Public Prototype API

The current prototype builds `build/libsolana_ingress.a` and exposes `include/solana/ingress.h`.

The decoded event contains only schema-local fields. HFT-specific event models, fixed-point types, queues, strategy state, and execution logic are not part of the public API.

## Not Yet Implemented

The following capabilities remain future work:

- signed serialized transaction submission;
- leader-schedule and validator-contact discovery;
- TPU QUIC/TLS transport and `solana-tpu` protocol negotiation;
- validator identity and stake-weighted QoS support;
- connection pooling and leader prewarming;
- adaptive leader routing and controlled fanout;
- retry, backpressure, and connection-failure handling;
- transaction landing or confirmation tracking;
- Agave and Firedancer TPU-ingress interoperability testing;
- kernel-bypass networking.

## Target Boundary

The intended TPU library will accept opaque signed serialized Solana transactions and deliver them to appropriate validator TPU ingress endpoints.

Transaction construction, signing, trading strategy, portfolio state, and private execution-engine behavior remain outside the transport library.

## Build

Requirements:

- a POSIX environment supported by the prototype;
- GCC or Clang with C11 support;
- POSIX sockets and pthreads.

Build and test:

```bash
make test
```

Run the decoder microbenchmark:

```bash
make bench
```

The test exercises the loopback UDP prototype. It is not a Solana cluster or TPU integration test.

## Architecture Records

- [ADR-0001: Fixed-Layout Trade-Event Parser Prototype](docs/architecture/0001-zero-allocation-borsh-deserialization.md)
- [ADR-0002: Use C11 for the Native Integration Layer](docs/architecture/0002-use-c11-for-gateway-performance.md)

## License

Apache License 2.0.
