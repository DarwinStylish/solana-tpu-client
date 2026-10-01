# Changelog

Notable changes to Solana TPU Client are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
The project has not published a versioned release.

## [Unreleased]

### Added

- Standalone public C11 ingress library and fixed-layout decoder.
- Public transaction-delivery C ABI with opaque client, request, and attempt identifiers.
- Structural topology validation with extensible record and stride semantics.
- Transactional copy-on-install topology ownership.
- Deterministic topology resolution and bounded route planning.
- Monotonic topology-freshness admission.
- Callable local transaction submission with request-owned transaction and target state.
- Bounded request-event retention and nonblocking caller-driven event polling.
- Explicit caller-owned discovery-provider refresh.
- GCC and Clang build and test coverage.
- AddressSanitizer and UndefinedBehaviorSanitizer coverage.
- Deterministic decoder and topology-validation fuzz smoke tests.
- Public-only discovery and delivery conformance tests.

### Changed

- The repository builds and tests independently from a standalone checkout.
- Public submission success represents local request acceptance only. It does not imply
  transport progress, validator receipt, transaction landing, or confirmation.
- Public ownership, lifetime, topology, discovery, request, and event contracts are
  explicitly defined and tested.

### Fixed

- Hardened ingress input validation, callback lifetime semantics, and ABI compatibility checks.
- Hardened failure atomicity for topology installation, discovery refresh, and local request acceptance.
