# Solana TPU Client

Solana TPU Client is a standalone native C library defining a transaction-delivery
boundary for signed serialized Solana transactions.

The current revision implements topology management, caller-driven discovery refresh,
deterministic route preparation, local submission acceptance, and delivery-event polling.
It does not transmit transactions to validator TPU endpoints.

## Status

The project is pre-release.

`solana_delivery_client_submit` performs local request acceptance. A successful return
means that the transaction and selected targets entered library-owned state and the
corresponding accepted event was retained. It does not imply transport progress,
validator receipt, transaction landing, or confirmation.

A separate ingress prototype remains available for fixed-layout local event decoding and
loopback UDP integration.

See [CURRENT_STATE.md](CURRENT_STATE.md) for the exact implemented boundary.

## Public Interfaces

| Header | Responsibility |
| --- | --- |
| `include/solana/delivery.h` | Delivery client, topology, submission, status, and event ABI |
| `include/solana/discovery.h` | Caller-owned topology discovery-provider ABI |
| `include/solana/ingress.h` | Fixed-layout ingress prototype API |

Transaction construction and signing remain caller responsibilities.

Application strategy, portfolio state, wallet behavior, and application-specific runtime
types are outside the library boundary.

## Build

Requirements:

- a POSIX environment;
- GCC/G++ or Clang/Clang++;
- GNU Make;
- C11 and C++17 compiler support.

```bash
make all
make test
```

Run the deterministic fuzz smoke targets with:

```bash
make FUZZ_CC=clang fuzz-smoke
```

The build produces the ingress and delivery static libraries under `build/`.

## Documentation

- [Current implementation](CURRENT_STATE.md)
- [Documentation index](docs/README.md)
- [Architecture overview](docs/architecture/README.md)
- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Changelog](CHANGELOG.md)

## License

Apache License 2.0.
