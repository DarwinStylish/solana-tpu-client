# Solana TPU Client

High-performance, kernel-bypass execution client for the Solana network.

## Overview

This adapter parses raw Solana Borsh-encoded trade events into the unified `event_t` struct used by the core execution engine. The parser is header-only (`adapter_solana.h` in `hft_core/include`) for zero-overhead inlining.

## Features

- **Borsh-to-C zero-copy deserialization**: Direct struct casting from wire bytes
- **Millisecond-to-nanosecond timestamp normalization**: Converts Solana's ms timestamps to the engine's ns resolution
- **Deterministic latency**: No allocations, no branching on the happy path

## Grant Milestone Roadmap

- [x] Milestone 1: Borsh-to-C deserializer and state sync verification
- [ ] Milestone 2: TPU injection and Ed25519 optimized signing loop
- [ ] Milestone 3: Mainnet shadow-mode deployment and security audit

## Integration

This module is consumed as a Git submodule by `hft_orchestrator`. All shared headers live in `hft_core/include`.

## Build & Test

```bash
make test
```

## License

Apache 2.0 — see the root [hft_orchestrator LICENSE](https://github.com/DarwinStylish/hft_orchestrator/blob/main/LICENSE).
