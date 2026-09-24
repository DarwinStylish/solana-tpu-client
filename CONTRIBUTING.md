# Contributing to Solana TPU Client

Thank you for contributing to Solana TPU Client.

This repository contains a standalone native C library. Contributions should preserve
its public-library boundary and avoid application-specific coupling.

## Prerequisites

A development environment should provide:

- a POSIX environment;
- GCC/G++ or Clang/Clang++;
- GNU Make;
- C11 and C++17 compiler support;
- Clang for the libFuzzer smoke targets.

## Getting Started

```bash
git clone https://github.com/DarwinStylish/solana-tpu-client.git
cd solana-tpu-client
make all
make test
make FUZZ_CC=clang fuzz-smoke
```

The repository builds and tests independently from a standalone checkout.

## Repository Boundary

Public interfaces live under `include/solana/`.

The public library does not depend on:

- private execution-engine types;
- trading or application strategy;
- portfolio or position state;
- wallet or signing implementations;
- application-specific queues or runtime types.

Transaction construction and signing are caller responsibilities.

## Branches

Create focused branches from `main` using lowercase kebab-case names.

Examples:

```text
feat/transport-backend
fix/topology-validation
docs/api-reference
test/submission-conformance
```

Do not push directly to `main`.

## Commit Messages

Use Conventional Commits:

```text
<type>[optional scope]: <description>
```

| Type | Purpose |
| --- | --- |
| `feat` | New behavior |
| `fix` | Defect correction |
| `perf` | Performance change |
| `refactor` | Behavior-preserving restructuring |
| `test` | Test or conformance coverage |
| `docs` | Documentation-only change |
| `ci` | Continuous-integration change |
| `build` | Build-system change |
| `chore` | Repository maintenance |

Keep commits atomic and use imperative subject lines.

## Pull Requests

Before opening a pull request:

1. review the complete diff;
2. keep commits scoped and conventional;
3. build and test with the relevant supported toolchains;
4. run fuzz smoke tests when parser or topology-validation behavior changes;
5. update documentation when public behavior, ABI, ownership, lifetime, or compatibility semantics change;
6. verify that no private repository dependency has been introduced.

A representative local verification sequence is:

```bash
make clean
make CC=gcc CXX=g++ all
make CC=gcc CXX=g++ test

make clean
make CC=clang CXX=clang++ all
make CC=clang CXX=clang++ test

make FUZZ_CC=clang fuzz-smoke
```

Continuous integration additionally runs AddressSanitizer, UndefinedBehaviorSanitizer,
repository-isolation checks, and a build from a tracked archive.

## C and ABI Standards

- Production C code uses strict C11.
- C++ linkage tests use C++17.
- Public ABI records use fixed-width integer types where representation matters.
- Extensible public records preserve documented `struct_size`, stride, reserved-field,
  and compatibility semantics.
- Ownership and lifetime rules must be explicit.
- Allocation is permitted where required by documented ownership semantics.
- Public API changes require corresponding ABI or conformance coverage.
- Platform-specific implementation details must not leak into the public ABI without
  an explicit contract.

## Testing

Changes should include coverage appropriate to the affected behavior.

Prefer deterministic unit tests, negative validation tests, failure-atomicity tests,
public-only conformance tests, sanitizer coverage, and fuzzing for pure input-validation
boundaries.

## Documentation

Public behavior should be documented at the same abstraction level at which it is exposed.

Architecture decision records capture durable architectural decisions.

## License

By contributing, you agree that your contributions will be licensed under the
[Apache License 2.0](LICENSE).
