# 2. Use C11 for Gateway Performance

Date: 2026-07-12

## Status

Accepted

## Context

The core requirement of this high-frequency trading (HFT) infrastructure is extreme low latency and deterministic execution. The gateway must process incoming market data at sub-microsecond speeds and pass it to the core engine.

While the wider Solana ecosystem heavily utilizes Rust, Rust's reliance on safe abstractions can sometimes obscure exact memory layouts and introduce hidden branching. C++ introduces complex name mangling, exceptions, and the potential for implicit allocations.

To achieve our performance goals, we require absolute control over memory layout, cache-line alignment, and a strict guarantee of zero dynamic allocations on the hot path.

## Decision

We use **C11** as the primary language for this ingestion gateway. 

We mandate the following constraints:
- **Zero Allocations:** No `malloc`, `free`, or `calloc` may be used anywhere on the hot path. All state must be statically allocated.
- **Compiler Hardening:** The code must compile cleanly with `-O3 -Wall -Wextra -Werror -std=c11 -fstack-protector-strong -D_FORTIFY_SOURCE=2`.

## Consequences

**Positive:**
- Unmatched performance and predictable cache locality.
- Extremely lightweight binaries with no runtime overhead.
- Direct memory mapping of network buffers to C-structs.

**Negative:**
- Lack of modern language safety features requires rigorous manual review.
- We must heavily rely on sanitizers (ASan, UBSan) in our CI pipelines to guarantee memory safety.
