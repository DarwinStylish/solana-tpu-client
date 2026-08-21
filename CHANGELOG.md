# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- Buffer length validation for all network adapter parsers (Solana, Hyperliquid, Monad)
- Exchange sequence ID tracking across all adapter parsers
- Compile-time `_Static_assert` for 64-byte cache-line struct enforcement
- AddressSanitizer and UndefinedBehaviorSanitizer build targets (`make test-asan`, `make test-ubsan`)
- Expanded fuzzer coverage: EVENT_UNKNOWN sentinel, negative prices
- Padding zeroing in adapter parsers for deterministic binary replay
- Security hardening compiler flags (`-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `-Wextra`)
- `SECURITY.md` responsible disclosure policy
- `CHANGELOG.md` per Keep a Changelog standard

### Fixed
- **CRITICAL**: Monad adapter double-scaling bug — price was scaled by SCALE² (10^16) instead of SCALE (10^8)
- Engine shutdown signal changed from `volatile bool*` to `_Atomic bool` with `memory_order_relaxed` for ARM/Graviton correctness
- `__attribute__((aligned(64)))` placement moved to standard typedef position for portability
- Strict aliasing violations in adapter test files resolved
- Non-portable `%lu` format specifiers replaced with `PRIu64`
- `check_size.c` relocated from `build/` to `tests/` to survive `make clean`

### Changed
- Adapter parser APIs now require `wire_len` and `seq_id` parameters (breaking API change)
- Fuzzer now generates `event_type=0` (EVENT_UNKNOWN) and negative prices
- All Makefiles upgraded with `-Wextra` and hardening flags

### Removed
- Committed ELF binaries from all repositories
- Stale `.backup` directories from orchestrator
- Duplicate header files from gateway submodule directories

## [0.1.0] - 2026-08-18
### Added
- Initial release of zero-allocation deterministic execution engine
- 128-bit fixed-point math library with symmetric rounding
- Lock-free SPSC ring buffer with C11 atomics
- Network adapters: Solana Borsh, Hyperliquid SBE, Monad EVM
- 1M-event chaos fuzzer with margin invariant assertions
- Apache 2.0 license
