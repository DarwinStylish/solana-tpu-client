# Solana TPU Client

Status: pre-transport prototype.

This repository is intended to evolve into a native Solana TPU transaction-delivery library. The current implementation does not submit transactions to validator TPU endpoints and does not implement Solana TPU QUIC transport.

## Current Implementation

The repository currently contains an experimental ingress path used with the HFT execution research stack:

- a non-blocking local UDP receiver;
- a fixed-layout, program-specific trade-event decoder;
- translation into the private engine `event_t` representation;
- enqueue into the engine SPSC ring buffer;
- a localhost UDP integration test;
- a single-threaded parser-and-queue microbenchmark.

The current parser and shared event types live in the sibling private `hft_core` repository. As a result, this prototype is not yet a standalone public library.

See [CURRENT_STATE.md](CURRENT_STATE.md) for the exact implementation boundary.

## Not Yet Implemented

The following capabilities are target functionality and are not part of the current implementation:

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

## Design Direction

The intended public boundary is a standalone native transport library that accepts opaque signed serialized Solana transactions and delivers them to appropriate TPU ingress endpoints. HFT strategy, execution logic, and private engine state are outside that boundary.

The transport design is intended to remain independent of a particular transaction version wherever possible. Transaction construction and signing belong to the calling application.

## Current Build Requirements

- Linux
- GCC or Clang with C11 support
- POSIX sockets and pthreads
- sibling `hft_core` headers from the current research workspace

The dependency on `hft_core` is part of the current prototype architecture and prevents this revision from being a standalone TPU client.

## Build and Test

```bash
make test
```

The test exercises the current localhost UDP ingress prototype. It is not a Solana cluster or TPU integration test.

The microbenchmark can be run with:

```bash
make bench
```

See [tests/README.md](tests/README.md) before interpreting benchmark results.

## Architecture Records

- [ADR-0001: Fixed-Layout Trade-Event Parser Prototype](docs/architecture/0001-zero-allocation-borsh-deserialization.md)
- [ADR-0002: Use C11 for the Native Integration Layer](docs/architecture/0002-use-c11-for-gateway-performance.md)

## License

Apache License 2.0.
