# Fuzzing

The public fixed-layout decoder has a libFuzzer harness in `tests/fuzz_decode.c`.

The harness exercises arbitrary byte strings and checks the decoder contract:

- every input length other than 33 bytes is rejected;
- at 33 bytes, only side values 0 and 1 are accepted;
- accepted multi-byte fields decode as little-endian integers;
- the caller-provided ingress sequence identifier is preserved;
- reserved ABI bytes are zero.

Run the bounded deterministic smoke test with:

```bash
make fuzz-smoke
```

The smoke target uses a fixed seed, run count, and 65-byte maximum input length so each CI invocation is bounded and repeatable within a given toolchain. It is a CI guard, not evidence that the decoder has been exhaustively proven correct.

For longer local exploration, build `build/fuzz_decode` and invoke it with additional libFuzzer options or a corpus directory.

The socket-based local ingress loop is intentionally outside this fuzz target. This harness focuses on the pure decoder boundary.

## Topology Validation

`tests/fuzz_topology.c` exercises the pure delivery-topology structural validator.

The harness uses only real bounded local storage for non-null topology arrays. It does not synthesize arbitrary process pointers or claim that the validator can prove the allocation extent behind a caller pointer.

It varies:

- zero, base-layout, extended-layout, null, and structurally invalid array configurations;
- element counts bounded by the storage supplied by the harness;
- base and extended strides;
- element `struct_size` values;
- reserved fields;
- endpoint address families, transports, roles, ports, and address bytes;
- validator-to-endpoint references;
- leader references and slot ranges;
- top-level topology size and reserved state.

The harness checks that validation is deterministic and returns only statuses currently defined for structural validation.

`make fuzz-smoke` runs bounded deterministic smoke tests for both the decoder and topology validator.
