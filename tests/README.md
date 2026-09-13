# Benchmark Methodology and Caveats

The benchmark in this directory evaluates only the standalone fixed-layout decoder.

It is a synthetic local microbenchmark. It is not a Solana TPU transport benchmark.

## Measured Path

Each iteration decodes the same preallocated 33-byte synthetic trade-event payload into a public ingress event structure.

A checksum derived from decoded fields is printed so the benchmark computation remains observable.

## What It Can Establish

The benchmark can compare decoder changes under the same machine, compiler, build flags, and workload.

The decoder performs no heap allocation.

## What It Does Not Measure

The benchmark does not measure:

- UDP socket receive cost;
- NIC or kernel networking latency;
- QUIC or TLS processing;
- Solana TPU transaction submission;
- leader routing;
- validator processing;
- transaction propagation or landing;
- end-to-end application latency.

The same input remains cache-hot during the loop.

## Interpreting Results

Results are environment-specific and should be reported with CPU, operating system, compiler, compiler flags, frequency policy, sample count, and measurement methodology.

The executable reports an arithmetic mean. It does not report percentile latency distributions.
