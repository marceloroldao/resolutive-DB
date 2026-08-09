# BDR V4 — Scheduler 2D rho_R x phi

Date: 2026-08-09

## Objective

Test whether adding phase to the CPU/shard routing rule improves the best previously measured BDR V4 partitioning strategy.

Baseline routing:

```text
core = rho_R mod number_of_cores
```

Experimental 2D routing:

```text
core = (phi + mix64(rho_R xor salt)) mod number_of_cores
```

The rule for architectural promotion is conservative: phase routing is promoted only if it improves throughput and/or tail latency consistently across relevant workloads. Otherwise the rho_R-only partitioning remains the mainline baseline.

## Reproducibility

Source:

```text
benchmarks/bdr_v4_phase2d_scheduler.cpp
```

Compile:

```bash
g++ -O3 -std=c++17 -pthread benchmarks/bdr_v4_phase2d_scheduler.cpp -o bdr_phase2d
```

Run:

```bash
./bdr_phase2d
```

Default workload:

- N = 300,000 records
- rho buckets = 10,000
- phase buckets = 65,536
- threads = 2, 4, 5
- 50,000 operations per thread
- 7 repetitions per configuration
- reported statistic = median throughput
- workloads = 100% reads and 90% reads / 10% writes

## Results from this session

| Threads | Write ratio | rho-only sharding (Mops/s) | rho-phi 2D (Mops/s) | Relative result |
|---:|---:|---:|---:|---:|
| 2 | 0% | 3.906 | 3.814 | -2.4% |
| 2 | 10% | 1.380 | 1.417 | +2.7% |
| 4 | 0% | 7.883 | 8.143 | +3.3% |
| 4 | 10% | 1.919 | 2.024 | +5.5% |
| 5 | 0% | 7.277 | 8.665 | +19.1% |
| 5 | 10% | 2.183 | 2.149 | -1.6% |

## Interpretation

The 2D scheduler produced improvements in several read-heavy configurations, especially the 5-thread read-only case. However, the improvement was not consistent across all configurations. It lost to rho-only routing in the 2-thread read-only workload and in the 5-thread mixed workload.

Therefore this experiment does **not** justify making phase-aware scheduling the default BDR architecture.

The currently preferred baseline remains the V4 rho_R-partitioned design, because it has the strongest combined evidence so far for throughput, low contention, and stable p99 behavior.

Phase-aware routing remains an experimental optimization that may be useful when workload distribution or CPU topology makes phi a good affinity signal.

## Engineering decision

Mainline baseline:

```text
rho_R -> partition -> local synchronization -> local table
```

Experimental branch only:

```text
(rho_R, phi) -> routing function -> partition/core
```

The phase dimension should not add complexity unless repeated benchmarks demonstrate a clear advantage over the rho_R-only baseline.

## Next tests

1. Optimize memory consumption of the rho_R-partitioned V4 baseline.
2. Compare against a properly sharded conventional hash table under the same number of locks/shards.
3. Evaluate Zipfian/hot-key workloads instead of only uniform random access.
4. Measure resize behavior and long-running mixed workloads.
5. Move from synthetic key/value microbenchmarks toward SQLite/PostgreSQL/Redis-style point lookup workloads where applicable.
