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

## Public Ingress Prototype API

The current prototype builds `build/libsolana_ingress.a` and exposes `include/solana/ingress.h`.

The decoded event contains only schema-local fields. HFT-specific event models, fixed-point types, queues, strategy state, and execution logic are not part of the public API.

The current prototype contract is intentionally narrow:

- `solana_ingress_decode` accepts exactly one 33-byte prototype event;
- shorter or longer buffers are rejected;
- the side discriminator accepts only the documented buy and sell values;
- the instruction discriminator is carried through as an opaque value and is not semantically validated;
- callback event storage is valid only for the duration of the callback;
- callers must copy an event if they retain it after the callback returns;
- callback and control contexts remain owned by the caller;
- `solana_ingress_run_local` returns `0` on requested shutdown and `-1` on failure, with `errno` identifying the failure;
- the current event structure occupies 48 bytes and includes reserved bytes that consumers must not interpret.

The project is pre-release. The current layout is tested explicitly to detect accidental ABI changes, but it is not yet declared a permanent version-1 ABI.

## Delivery API and Topology State

`include/solana/delivery.h` defines the Phase 1 transaction-delivery ABI vocabulary and the current client/topology-state and local-submission boundary.

It currently provides:

- ABI version constants;
- an opaque delivery-client declaration;
- fixed-width API status, request-ID, and attempt-ID types;
- validator identity and endpoint representations;
- explicit validator-to-endpoint associations;
- leader and topology snapshot records;
- an extensible submission-options structure carrying maximum topology age and bounded target policy;
- a request/attempt/observation event envelope.

The delivery boundary builds `build/libsolana_delivery.a` and currently exposes:

- `solana_delivery_topology_validate`;
- `solana_delivery_client_create`;
- `solana_delivery_client_destroy`;
- `solana_delivery_client_install_topology`;
- `solana_delivery_client_submit`;
- `solana_delivery_client_poll_events`.

The validator checks the structural integrity of caller-supplied topology views, including:

- public structure-size prefixes;
- explicit array strides;
- array pointer/count consistency;
- element `struct_size` compatibility with the supplied stride;
- alignment requirements;
- reserved fields;
- endpoint address-family, transport, and role discriminators;
- endpoint port and IPv4 representation rules;
- validator-to-endpoint references;
- leader validator references and slot-range ordering.

Topology validation is pure and does not install topology, perform discovery, select routes, open connections, submit transactions, or report landing.

Topology installation uses copy-on-install ownership. On success, the client owns normalized copies of the ABI prefixes understood by this implementation and retains no caller array pointers. Caller snapshot storage may therefore be reused after the installation call returns success.

The first installed snapshot may use any generation value. Later installations must use a strictly greater generation; equal or lower generations return `SOLANA_DELIVERY_STATUS_TOPOLOGY_STALE`. Each successful installation records a local monotonic receipt time.

Installation is transactional. Structural rejection, allocation failure, or monotonic-clock failure leaves the previously installed snapshot unchanged.

An empty topology is structurally valid and may be installed, but that does not imply that a usable route exists.

The delivery library also has an internal deterministic topology resolver. Given an installed snapshot and requested slot, it resolves matching leader records through validator-to-endpoint associations while preserving source order. It performs no ranking, deduplication, freshness policy, fanout, retries, allocation, or network activity. This resolver is not part of the public C ABI.

An internal bounded route planner now consumes those resolved candidates. It deduplicates by validator-and-endpoint identity, preserves first-occurrence ordering and leader provenance, and applies a positive target limit without allocation or network activity. It remains internal and does not define a public routing-policy ABI.

An internal submission-policy evaluator now checks installed-topology freshness using the library monotonic clock domain. It requires a positive maximum topology age and target limit, distinguishes unavailable from stale topology, and does not interpret topology generation or caller-observed slot context as elapsed time.

`solana_delivery_client_submit` now provides callable local request acceptance. It validates the public submission options, evaluates topology freshness, resolves the installed snapshot `current_slot`, applies the deterministic bounded route planner, copies the caller transaction bytes, materializes request-owned validator identities and endpoints, and assigns a nonzero request identifier. Accepted request state remains independent of both caller-buffer lifetime and later topology replacement.

Successful local acceptance also retains one request-level `SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED` event with request sequence one and no transport-attempt identifier. Request acceptance and event retention are one logical commit, and a full bounded event channel rejects the submission with `SOLANA_DELIVERY_STATUS_RESOURCE_EXHAUSTED` before the request or identifier is committed.

`solana_delivery_client_poll_events` provides nonblocking caller-driven FIFO event polling with explicit output stride, partial drains, and zero-consumption empty or zero-capacity polling. Polling the accepted event does not reclaim the request.

`SOLANA_DELIVERY_STATUS_OK` from submission means only that the request entered library-owned local state and its accepted event was retained. The current implementation does not create transport attempts or send transaction bytes to a validator.

Discovery, adaptive routing beyond the literal current-slot plan, retries, connection management, transport, transport attempts, terminal request transitions and reclamation, and observation behavior are not implemented.

## Not Yet Implemented

The following capabilities remain future work:

- transport of locally accepted signed serialized transactions to validator TPU endpoints;
- leader-schedule and validator-contact discovery;
- TPU QUIC/TLS transport and `solana-tpu` protocol negotiation;
- validator identity and stake-weighted QoS support;
- connection pooling and leader prewarming;
- adaptive leader routing and controlled fanout;
- transport retry, transport backpressure, and connection-failure handling;
- terminal request transitions and post-terminal request reclamation;
- transaction landing or confirmation tracking;
- Agave and Firedancer TPU-ingress interoperability testing;
- kernel-bypass networking.

## Target Boundary

The intended TPU library will accept opaque signed serialized Solana transactions and deliver them to appropriate validator TPU ingress endpoints.

Transaction construction, signing, trading strategy, portfolio state, and private execution-engine behavior remain outside the transport library.

## Build

Requirements:

- a POSIX environment supported by the prototype;
- GCC/G++ or Clang/Clang++ with C11 and C++17 support;
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
- [ADR-0003: Signed Transaction Delivery Boundary](docs/architecture/0003-transaction-delivery-boundary.md)
- [ADR-0004: Delivery Status Semantics](docs/architecture/0004-delivery-status-semantics.md)
- [ADR-0005: Separate Discovery, Routing, Transport, and Observation](docs/architecture/0005-topology-routing-transport-separation.md)
- [ADR-0006: Stable C ABI for Transaction Delivery](docs/architecture/0006-stable-delivery-c-abi.md)
- [ADR-0007: Topology Snapshot Contract](docs/architecture/0007-topology-snapshot-contract.md)
- [ADR-0008: Request and Attempt Event Model](docs/architecture/0008-request-attempt-event-model.md)
- [ADR-0009: Deterministic Topology Resolution](docs/architecture/0009-topology-resolution.md)
- [ADR-0010: Deterministic Bounded Route Planning](docs/architecture/0010-route-planner-policy.md)
- [ADR-0011: Submission Admission and Topology Freshness](docs/architecture/0011-submission-admission-freshness.md)
- [ADR-0012: Callable Submission Contract](docs/architecture/0012-callable-submission-contract.md)
- [ADR-0013: Request Lifecycle and Event Polling](docs/architecture/0013-request-lifecycle-and-event-polling.md)
- [ADR-0014: Use Explicit Caller-Owned Discovery Providers](docs/architecture/0014-discovery-provider-interface.md)

## Technical Roadmap

See [ROADMAP.md](ROADMAP.md) for the engineering phases of the transaction-delivery architecture.

The roadmap describes target work and does not imply that unimplemented transport capabilities exist in the current release.

## License

Apache License 2.0.
