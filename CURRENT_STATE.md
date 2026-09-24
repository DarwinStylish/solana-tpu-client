# Current State

This document describes the behavior implemented by the current revision.

## Delivery Boundary

The public delivery interface provides:

- fixed-width delivery status, request identifier, and attempt identifier types;
- validator identity and endpoint records;
- validator-to-endpoint associations;
- leader ranges and topology snapshots;
- extensible submission options;
- request, attempt, and observation event vocabulary;
- opaque delivery-client lifecycle operations.

Topology validation is structural. It checks public record prefixes, strides, alignment,
reserved fields, endpoint discriminators, endpoint representation, association indices,
leader references, and leader-range ordering.

Topology installation internalizes the ABI prefixes understood by the implementation.
Caller-owned topology arrays are not retained after successful installation.

The first installed topology may use any generation value. Each later installation must
use a strictly greater generation. Installation is failure-atomic: validation, allocation,
copying, generation checks, or clock failure leave the previously installed topology
unchanged.

## Discovery

`include/solana/discovery.h` defines a caller-owned discovery-provider interface.

`solana_delivery_client_refresh_topology` performs explicit synchronous refresh. A
successfully acquired provider snapshot is passed through the normal topology-installation
path and released after the installation attempt. The delivery client retains neither the
provider nor provider-owned snapshot storage.

No concrete RPC, streaming, or Geyser discovery provider is included.

## Routing and Admission

The implementation contains an internal deterministic topology resolver and bounded route
planner.

Resolution preserves leader-array order and validator-to-endpoint association order.
Planning deduplicates validator-and-endpoint targets, preserves first-occurrence order and
leader provenance, and applies the configured positive target limit.

Submission admission evaluates topology freshness using the client monotonic clock domain.
Topology generation and caller-observed slot context are not treated as elapsed-time
clocks.

The installed topology `current_slot` is used as the literal routing anchor for local
submission preparation.

## Submission and Events

`solana_delivery_client_submit` accepts a non-empty caller transaction byte range and, on
success:

- copies the transaction into library-owned storage;
- materializes the selected validator identities and endpoint records;
- records the topology generation and routing slot used for the request;
- assigns a nonzero request identifier unique within the client lifetime;
- retains one `SOLANA_DELIVERY_REQUEST_EVENT_ACCEPTED` event.

Request acceptance and accepted-event retention form one logical commit. Failure before
that commit does not create a partial request or consume a request identifier.

`solana_delivery_client_poll_events` is nonblocking and copies retained events into
caller-provided stride-aware output storage in FIFO order. Partial drains are supported.
Zero-capacity or empty polling consumes no events.

Polling an accepted event does not reclaim the corresponding request. Accepted request
storage is released when the client is destroyed.

`SOLANA_DELIVERY_STATUS_OK` from submission means local acceptance only. The current
implementation creates no transport attempt and sends no transaction bytes to a validator.

## Ingress Prototype

The separate ingress prototype provides:

- a fixed-layout 33-byte little-endian event decoder;
- bounds and side-discriminator validation;
- a loopback-only nonblocking UDP receiver;
- synchronous callback delivery;
- a 48-byte public event record with reserved bytes;
- a synthetic decoder microbenchmark.

Callback event storage is valid only for the duration of the callback. Event and lifecycle
contexts remain caller-owned.

The decoder is not a Solana transaction decoder, a general Borsh implementation, or the
Solana TPU wire protocol.

## Not Implemented

The current revision does not implement:

- validator TPU transaction transmission;
- QUIC client transport or TLS identity handling;
- `solana-tpu` protocol negotiation;
- a concrete cluster discovery provider;
- connection pooling or leader prewarming;
- stake-weighted QoS behavior;
- adaptive or transport-aware routing beyond the literal installed routing context;
- transport retry or transport-attempt lifecycle;
- terminal request transitions or post-terminal request reclamation;
- landing or confirmation observation;
- Agave or Firedancer TPU-ingress conformance;
- kernel-bypass networking.

## Verification

The repository includes:

- GCC and Clang builds;
- public-only delivery and discovery conformance tests;
- C++ linkage and public-signature checks;
- AddressSanitizer and UndefinedBehaviorSanitizer coverage;
- deterministic decoder and topology-validation fuzz smoke targets;
- repository-isolation checks;
- tracked-archive build verification.

See [docs/architecture/README.md](docs/architecture/README.md) for the architecture
overview and decision records.
