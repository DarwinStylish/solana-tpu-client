# Decoder Fuzzing

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

The smoke target uses a fixed seed and run count for reproducibility. It is a CI guard, not evidence that the decoder has been exhaustively proven correct.

For longer local exploration, build `build/fuzz_decode` and invoke it with additional libFuzzer options or a corpus directory.

The socket-based local ingress loop is intentionally outside this fuzz target. This harness focuses on the pure decoder boundary.
