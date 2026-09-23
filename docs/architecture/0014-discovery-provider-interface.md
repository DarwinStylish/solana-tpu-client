# 14. Use Explicit Caller-Owned Discovery Providers

Date: 2026-09-23

## Status

Accepted.

## Context

The delivery client already consumes coherent topology snapshots through
`solana_delivery_client_install_topology`.

Topology installation is the existing authority for structural validation,
generation ordering, copy-on-install ownership, and local monotonic receipt
time.

Discovery sources may vary. A topology snapshot could come from RPC polling,
application-owned cache state, a streaming adapter, deterministic tests, or
another external source.

Permanently attaching one discovery mechanism to the delivery client would
couple topology acquisition to client lifetime. Performing discovery
implicitly during submission would also introduce hidden I/O and latency into
the submission path.

## Decision

Discovery providers are caller-owned and implementation-independent.

Topology refresh is explicit and synchronous. A provider supplies a borrowed
public `solana_delivery_topology_t` snapshot for the duration of one refresh
operation.

The refresh path passes that snapshot through the existing topology
installation boundary. The client does not retain the provider, provider
context, topology pointer, or provider-owned topology arrays after refresh
returns.

Transaction submission never performs implicit discovery.

The generation carried by the provider snapshot remains subject to the
existing client topology-generation contract. Discovery does not introduce a
second generation scheme.

## Consequences

Discovery implementations can evolve independently from routing, transport,
and submission.

Submission retains deterministic behavior over already-installed client state
and does not acquire hidden discovery latency or network I/O.

Provider implementations may use temporary storage because successful
topology installation internalizes the snapshot before the provider borrow is
released.

Callers that switch discovery sources for an existing client must preserve
generation values compatible with that client topology history.

Blocking discovery work, when present, occurs only during an explicit refresh
operation.

## Alternatives Considered

- retain a discovery provider for the lifetime of each delivery client;
- invoke discovery implicitly from transaction submission;
- expose only manual topology installation and define no discovery-provider
  abstraction.
