# Documentation

The documentation is organized by responsibility.

## Project State

- [Current implementation](../CURRENT_STATE.md) — exact behavior implemented by the current revision.
- [Changelog](../CHANGELOG.md) — notable repository changes.

## Architecture

- [Architecture overview](architecture/README.md) — component boundaries, ownership flows,
  discovery, submission, event flow, and architecture decision records.

## Public Interfaces

- [`delivery.h`](../include/solana/delivery.h) — delivery-client, topology, submission, status, and event ABI.
- [`discovery.h`](../include/solana/discovery.h) — caller-owned discovery-provider ABI.
- [`ingress.h`](../include/solana/ingress.h) — fixed-layout ingress prototype API.

## Repository

- [Contributing](../CONTRIBUTING.md)
- [Security policy](../SECURITY.md)
- [License](../LICENSE)
