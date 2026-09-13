# 2. Use C11 for the Native Integration Layer

Date: 2026-07-12

## Status

Accepted.

## Context

The project targets an embeddable native transport boundary that can be consumed from systems software without requiring callers to adopt a specific application runtime.

The implementation requires explicit control over memory ownership, compact ABI-compatible interfaces, predictable data layout, and straightforward foreign-function integration.

## Decision

Use C11 for the native integration layer.

For the current parser hot path:

- no `malloc`, `calloc`, or `free` operations are performed;
- state is supplied by the caller or allocated during surrounding initialization;
- compilation uses strict warnings and hardening flags;
- sanitizer testing is used to detect memory and undefined-behavior defects.

The current build uses:

```text
-O3 -Wall -Wextra -Werror -std=c11 -fstack-protector-strong -D_FORTIFY_SOURCE=2
```

## Consequences

### Positive

- stable C interfaces are directly consumable from many systems languages;
- memory ownership remains explicit;
- the runtime dependency surface can remain small;
- low-level transport and buffer behavior can be controlled directly.

### Negative

- C does not provide automatic memory-safety guarantees;
- concurrency and lifetime rules require explicit review;
- sanitizers, fuzzing, static analysis, and integration tests are necessary complements to manual review.

This decision does not itself establish any latency target. Performance claims require measurements of the specific path being evaluated.
