# 1. Zero-Allocation Borsh Deserialization

Date: 2026-07-12

## Status

Accepted

## Context

Solana serializes network data using the Borsh specification. Standard Rust and C++ Borsh libraries dynamically allocate memory to build ASTs, Maps, or object graphs. In a high-frequency trading context, the latency spikes introduced by the OS memory allocator (malloc/free) are unacceptable.

## Decision

We implemented a custom, stateful byte-stream parser for Borsh payloads. 
- The parser advances a pointer through the raw UDP payload buffer.
- It extracts *only* the strictly required fields (price, quantity, order side).
- It writes these fields directly into the pre-allocated `event_t` struct, which is then passed to the core engine.

## Consequences

**Positive:**
- Extreme parsing speed and zero dynamic allocations, preserving strict sub-microsecond determinism.

**Negative (Mitigated):**
- Fragile coupling. The parser is strictly coupled to the exact byte-offset structure of the target smart contract. 
- *Mitigation:* We shifted this fragility from runtime to compile-time. A build-time Python script (`generate_solana_parser.py`) automatically ingests the smart contract's Anchor IDL (`solana_idl.json`) and generates the exact C pointer offsets (`borsh_offsets.h`). Any schema upgrade now safely and predictably triggers a build pipeline update, completely preventing runtime memory corruption.
