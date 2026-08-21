# Solana TPU Client


High-performance, kernel-bypass execution client for the Solana network.

## Overview

This adapter parses raw Solana Borsh-encoded trade events into the unified `event_t` struct used by the core execution engine. The parser is header-only (`adapter_solana.h` in `hft_core/include`) for zero-overhead inlining.

## Compiler Requirements
- **GCC**: >= 4.9 with C11 support.
- **Extensions**: `__int128` extension required (supported by GCC/Clang on x86-64 or aarch64).

## Features

- **Borsh-to-C zero-copy deserialization**: Direct struct casting from wire bytes
- **Millisecond-to-nanosecond timestamp normalization**: Converts Solana's ms timestamps to the engine's ns resolution
- **Deterministic latency**: No allocations, no branching on the happy path

## API Updates
Note: The adapter parser API has been updated. Calling the parsing methods now requires passing both the `wire_len` and `seq_id` parameters to properly support bounds checking and sequence tracking.



## Integration

This module is consumed as a Git submodule by `hft_orchestrator`. All shared headers live in `hft_core/include`.

## Build & Test

```bash
make test
```

## Governance & Architecture

* **Code of Conduct:** Please review our [Code of Conduct](CODE_OF_CONDUCT.md).
* **Architecture Decision Records (ADRs):** 
  * [ADR-0001: Zero-Allocation Borsh Deserialization](docs/architecture/0001-zero-allocation-borsh-deserialization.md)
  * [ADR-0002: Use C11 for Gateway Performance](docs/architecture/0002-use-c11-for-gateway-performance.md)

## License

This project is licensed under the Apache License 2.0.
