# 1. Fixed-Layout Trade-Event Parser Prototype

Date: 2026-07-12

## Status

Accepted for the current prototype.

## Context

The pre-transport prototype consumes one project-specific 33-byte little-endian trade-event layout.

The parser exists to evaluate a compact native decoding path independently of any private execution engine.

This payload is not the Solana TPU transaction-ingress protocol and the decoder is not a general Borsh implementation.

## Decision

The public decoder reads schema fields explicitly from byte offsets and produces a schema-local public event.

Multi-byte integers are reconstructed as little-endian values rather than loaded through potentially unaligned typed pointers.

The decoder validates:

- non-null input and output pointers;
- minimum payload length;
- the supported side discriminator.

It performs no heap allocation.

## Consequences

### Positive

- the prototype is independently buildable;
- decoding does not depend on host alignment behavior;
- decoding does not depend on host byte order;
- private HFT types are absent from the public interface.

### Limitations

- the schema remains project-specific;
- the decoder does not parse Solana transactions;
- the decoder does not implement TPU networking;
- the decoder does not authenticate payload origin;
- the instruction discriminator is exposed but not semantically validated by this prototype.

The future TPU transaction-delivery implementation is a separate transport concern.
