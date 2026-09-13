# 3. Signed Transaction Delivery Boundary

Date: 2026-09-13

## Status

Accepted as the target architecture.

The functionality described by this record is not implemented by the current prototype.

## Context

The existing repository contains a standalone fixed-layout inbound decoder prototype. That prototype is useful for exercising C ABI discipline, explicit byte decoding, testing, fuzzing, and repository isolation, but it is not the primary boundary of the intended TPU transaction-delivery library.

The target library is an outbound delivery component. Its responsibility begins after a caller has already constructed and signed a Solana transaction.

## Decision

The primary public delivery boundary will accept an opaque serialized transaction together with submission policy and will attempt delivery through supported Solana TPU ingress paths.

The transaction bytes are opaque to the delivery layer except for validation required by an explicitly defined transport or protocol contract.

The delivery layer does not construct, modify, or sign transactions.

### Caller responsibilities

The caller owns:

- transaction construction
- message compilation
- account selection
- blockhash selection
- fee and compute-budget policy
- signing and key custody
- trading or application strategy
- interpretation of execution results

### Delivery-library responsibilities

The delivery library owns:

- accepting an already-signed serialized transaction
- obtaining routing information through a discovery provider
- selecting delivery targets through a routing policy
- maintaining transport connections
- performing transport attempts
- applying bounded retry and backpressure policy
- exposing request-level and attempt-level telemetry
- reporting transport evidence without misrepresenting it as transaction landing

### Transaction-buffer contract

The future public ABI must accept a byte pointer and explicit length rather than encoding a project-specific transaction structure into the public interface.

The API must not expose the current 33-byte prototype event layout as a transaction representation.

The public ABI should not embed a legacy fixed transaction-size assumption. Bounds must be defined by the transaction and transport contracts implemented by the corresponding release.

The submission function does not transfer ownership of the caller buffer merely by receiving its pointer. If an asynchronous implementation requires bytes after the submission call returns, the implementation must retain them under an explicit ownership contract rather than silently retaining a borrowed pointer.

## Public and Private Boundary

The delivery library must remain independent of private execution-engine types.

It must not require:

- private `event_t` definitions
- private fixed-point arithmetic
- private SPSC queues
- private venue identifiers
- portfolio or position state
- strategy code

A private HFT engine may consume the library as an ordinary caller, but it is not part of the public library architecture.

## Excluded Scope

The transaction-delivery library is not:

- a wallet
- a key-management system
- a transaction builder
- a general Solana SDK
- a trading strategy engine
- a portfolio engine
- a Jito bundle client
- a hosted transaction relay
- a validator
- a Geyser implementation
- a custom QUIC implementation
- a kernel-bypass networking stack

Those exclusions keep the public boundary focused on transaction delivery.

## Consequences

The current inbound decoder and the future outbound transaction-delivery API are separate concerns.

The existing `solana_ingress_event_t` prototype therefore must not dictate the transaction-delivery ABI.

A future delivery API can be introduced alongside or after retirement of the prototype without importing private-engine semantics.
