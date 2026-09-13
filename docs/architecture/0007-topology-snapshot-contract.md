# 7. Topology Snapshot Contract

Date: 2026-09-13

## Status

Accepted as the Phase 1 topology design.

No topology API described by this record is implemented yet.

## Context

Routing requires leader and validator contact information, but transaction submission must not be hard-wired to one RPC, streaming, or external discovery mechanism.

The delivery core therefore needs a transport-independent input representation for current routing information.

## Decision

Phase 1 will define a caller-supplied topology snapshot boundary.

A discovery component obtains external cluster information and converts it into the public topology representation. The delivery library consumes that representation without needing to know how it was obtained.

A built-in discovery implementation may be added separately.

## Snapshot Semantics

A topology snapshot represents one coherent routing view supplied to the library.

It must not require the route planner to combine independently changing caller-owned arrays after installation.

On successful installation, the implementation must internalize the data it requires or otherwise establish an explicit ownership contract before returning.

The simplest Phase 1 contract is copy-on-install: caller-provided snapshot memory may be released or reused after the update call returns.

## Snapshot Identity

Each installed snapshot receives or carries a monotonically comparable generation value.

Generation identifies ordering of topology updates within one client instance. It is not a Solana slot and must not be interpreted as one.

The implementation must reject replacement of a newer installed snapshot with an older generation unless an explicit reset operation is defined.

## Freshness

Snapshot freshness and chain slot information are distinct.

The library records local receipt time for an installed snapshot using its own monotonic time source.

Routing policy may reject or degrade behavior when topology age exceeds configured policy.

Caller-supplied wall-clock timestamps must not be used as the sole basis for monotonic expiry calculations.

## Validator Identity

Validator identity is represented independently from network endpoints.

The identity representation must be able to carry one Solana validator public key without requiring textual base58 conversion inside the routing hot path.

A canonical 32-byte binary identity representation is appropriate for the public topology boundary.

## Endpoint Representation

Transport endpoints must not expose platform `sockaddr` structures in the stable public ABI.

The public endpoint representation must encode:

- address family
- address bytes
- port
- transport role or class when necessary

IPv4 and IPv6 must have canonical explicit representations.

Port values must have one documented host-or-network-order convention at the ABI boundary.

The implementation converts that representation to platform socket structures internally.

## Identity-to-Endpoint Mapping

A topology snapshot may associate one validator identity with multiple endpoints.

The architecture must also permit multiple validator identities to reference the same transport endpoint.

Therefore:

- validator identity is routing metadata
- transport endpoint identity is connection-pool metadata
- neither is used as an implicit substitute for the other

## Leader Information

Leader information associates slot or slot-range routing context with validator identity.

The route planner may construct a plausible-leader frontier from more than one leader entry.

The public submission API must not require the caller to identify exactly one validator target.

## Snapshot Validation

Installation must reject structurally invalid topology, including invalid array bounds, unsupported endpoint address families, malformed identity lengths, and internally inconsistent references.

Semantic validation must not silently invent missing leader or endpoint information.

## Discovery Separation

The topology snapshot API is not an RPC API.

RPC polling, streaming updates, Geyser consumers, test fixtures, or application-specific discovery systems may all produce snapshots through adapters.

This keeps the transaction-delivery core independently testable.

## Consequences

Phase 1 routing tests can operate entirely on deterministic synthetic snapshots.

Transport and discovery implementations can evolve independently of the caller-facing submission ABI.
