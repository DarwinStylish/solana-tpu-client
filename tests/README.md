# Benchmark Methodology and Caveats

The benchmark in this directory evaluates the current fixed-layout event decoder together with one SPSC enqueue/dequeue round trip.

It is a synthetic local microbenchmark. It is not a Solana TPU transport benchmark.

## Measured Path

Each iteration:

1. decodes the same preallocated synthetic trade-event payload;
2. enqueues the resulting `event_t` into the SPSC ring buffer;
3. immediately dequeues the event on the same thread.

The reported average therefore includes both parsing and queue operations.

## What It Can Establish

The benchmark can be used to compare local implementation changes to the current decoder and queue path under the same machine, compiler, build flags, and workload.

The current parser and queue path perform no heap allocation.

## What It Does Not Measure

The benchmark does not measure:

- UDP socket receive cost;
- NIC or kernel networking latency;
- QUIC or TLS processing;
- Solana TPU transaction submission;
- leader routing;
- cross-core synchronization;
- validator processing;
- transaction propagation or landing;
- end-to-end application latency.

The same payload is repeatedly reused and is expected to remain cache-hot. Producer and consumer operations also execute sequentially on one thread.

## Interpreting Results

Results are environment-specific and should be reported with:

- CPU model;
- operating system and kernel;
- compiler and version;
- compiler flags;
- CPU frequency policy;
- sample count;
- measurement methodology.

The current executable reports an arithmetic mean. It does not currently report percentile latency distributions.
