# BDR V5 — Compact Memory + High-Density/Zipf Benchmark

Date: 2026-08-09
Repository: `marceloroldao/resolutive-DB`

## Goal

Reduce the memory cost of BDR V4 while preserving lookup performance. The V4 eager Robin Hood layout used approximately 207.7 bytes/record in the prior experiment. V5 changes the local storage strategy to exact-size contiguous vectors per `rho_R` partition, sorted by compact local key.

## V5 layout

`key -> encoder -> rho_R -> contiguous local vector -> binary search on compact local key -> payload`

The local key is derived from `(phi, signature)`.

## Reproduction

Compile:

```bash
g++ -O3 -std=c++17 benchmarks/bdr_v5_compact_memory.cpp -o bdr_v5_compact
./bdr_v5_compact

g++ -O3 -std=c++17 benchmarks/bdr_v5_zipf.cpp -o bdr_v5_zipf
./bdr_v5_zipf
```

## Experiment A — 300,000 records

Environment execution results:

| Engine | Build time | Average uniform lookup | Approx. bytes | Approx. bytes/record |
|---|---:|---:|---:|---:|
| BDR V5 Compact | 0.0384 s | 0.2869 us | 5,040,000 | 16.8 |
| Flat `std::unordered_map` | 0.0846 s | 0.3797 us | 14,471,224 | 48.24 |

Relative to the previous V4 estimate (~207.7 bytes/record), V5 reduced index memory by roughly 12.4x in this configuration.

Important: these byte figures are structural estimates from container capacities and object sizes. They are not resident-set-size measurements and therefore must not be described as exact process memory usage.

## Experiment B — 1,000,000 records, M=10,000, lambda=100

| Engine | Distribution | Throughput | p50 | p99 |
|---|---|---:|---:|---:|
| BDR V5 Compact | uniform | 1.579 Mops/s | 0.600 us | 1.277 us |
| Flat hash | uniform | 1.316 Mops/s | 0.740 us | 2.052 us |
| BDR V5 Compact | Zipf-like hot keys | 1.671 Mops/s | 0.546 us | 1.380 us |
| Flat hash | Zipf-like hot keys | 1.541 Mops/s | 0.612 us | 1.251 us |

Approximate V5 structural index memory: 16.24 bytes/record at N=1,000,000.

## Interpretation

V5 is the strongest single-thread read-oriented candidate observed so far in the project because it combines substantially lower structural memory use with competitive or superior lookup throughput in the measured environment.

However, its local lookup is binary search over a partition of size `k`, so the local theoretical cost is `O(log k)`, not strict `O(1)`. With `M` fixed and `N` growing, `k` grows approximately with `lambda=N/M`.

Therefore V5 should not replace the concurrent V4 for all workloads. Current recommended split:

- V5 Compact: read-heavy / immutable or batch-built indexes where memory efficiency and cache locality dominate.
- V4 partitioned Robin Hood: write-heavy concurrent workloads where local locking and constant-expected local lookup dominate.

## Hot-key finding

Under the Zipf-like workload, V5 retained higher aggregate throughput than the flat hash in this execution, but the flat hash had slightly better p99. This means V5 does not dominate every tail-latency workload.

## Next tests

1. Repeat across multiple seeds and repetitions, report median and dispersion.
2. Sweep `M` and therefore lambda to identify where local binary search begins to dominate.
3. Test 90/10, 70/30, and 50/50 read/write workloads with an append/update-capable compact design.
4. Compare against a sharded `std::unordered_map`, a modern flat/Swiss hash implementation when available, SQLite, DuckDB, and eventually Redis/PostgreSQL using workload-appropriate protocols.
5. Measure RSS and hardware counters (cache misses, branch misses) on a dedicated Linux host.

## Scientific status

These results are empirical measurements from one execution environment and are not proof of asymptotic superiority. Claims must remain scoped to the tested implementation and workload.
