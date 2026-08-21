# Contributing to HFT Orchestrator

Thank you for your interest in contributing. This document outlines the workflow, standards, and conventions for the project.

## Prerequisites

- GCC with C11 support (`gcc >= 4.9`)
- Python 3 (for the fuzzer)
- GNU Make

## Getting Started

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/DarwinStylish/hft_orchestrator.git
cd hft_orchestrator

# Build and run tests
make test
```

## Branch Naming

Use lowercase `kebab-case` with the following format:

```
<type>/<short-description>
```

**Examples:**
- `feat/dpdk-kernel-bypass`
- `fix/equity-aggregation-bug`
- `docs/update-architecture-diagram`
- `perf/ring-buffer-cache-prefetch`

## Commit Messages

We follow [Conventional Commits](https://www.conventionalcommits.org/). Every commit message must use this format:

```
<type>[optional scope]: <description>
```

**Types:**
| Type | Usage |
|---|---|
| `feat` | A new feature |
| `fix` | A bug fix |
| `perf` | A performance improvement |
| `refactor` | Code restructuring (no feature or fix) |
| `test` | Adding or updating tests |
| `docs` | Documentation changes only |
| `chore` | Build system, CI, or maintenance |

**Rules:**
- Use imperative mood: `"Add DPDK driver"`, not `"Added DPDK driver"`
- Keep the subject line under 72 characters
- Denote breaking changes with `!`: `feat(api)!: remove v1 endpoints`

**Examples:**
```
feat(engine): add weighted-average entry pricing for DCA
fix(adapter): guard against division by zero in Monad price calc
perf(ring-buffer): add cache-line prefetch hint on dequeue
test(fuzz): add post-fuzz equity invariant assertion
```

## Pull Request Workflow

1. Fork the repository and create a branch from `main`
2. Make your changes with atomic commits
3. Ensure all tests pass: `make test`
4. Run the fuzzer to verify stability: `make fuzz`
5. Open a Pull Request against `main`
6. Request a review — direct pushes to `main` are not allowed

## Security Testing

Before submitting a Pull Request, you **must** run the address and undefined behavior sanitizers to ensure no memory safety violations are introduced:
```bash
make test-asan
make test-ubsan
```

## Code Standards

### C11

- **No heap allocations**: Do not use `malloc`, `calloc`, `realloc`, or `free`
- **No floating-point on hot paths**: Use `fixed_math.h` for all arithmetic
- **Cache-line alignment**: All shared structs must be 64-byte aligned. This MUST be enforced at compile time using `_Static_assert`
- **Include guards**: Use `#ifndef` / `#define` / `#endif` pattern
- **Doc comments**: All public functions must have `/** @brief ... */` documentation
- **Cross-thread Synchronization**: Use `_Atomic` instead of `volatile` for multi-threaded state
- **Formatting**: Use `PRIu64` instead of `%lu` when printing `uint64_t` types

### Network Adapters
- **Buffer Lengths**: Buffer length validation is strictly required for all network adapter parsers

### Testing

- All new features must include corresponding tests
- Tests use `assert()` for hard invariant checking
- Negative tests are required for all input validation paths

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](LICENSE).
