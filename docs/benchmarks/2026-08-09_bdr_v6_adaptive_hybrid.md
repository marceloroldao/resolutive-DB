# BDR V6 Adaptive Hybrid Benchmark — 2026-08-09

## Objective
Test a per-partition adaptive engine where each `rho_R` region selects either:

- **Compact**: contiguous sorted array + binary search, optimized for memory/read locality.
- **Robin Hood**: local open-addressing table, optimized for high-density / latency-sensitive partitions.

The selection rule is based on **local partition occupancy**, not global `N/M` alone.

## Environment and reproducibility
Reference implementation: `benchmarks/bdr_v6_adaptive_hybrid.cpp`.

Compile:

```bash
g++ -O3 -std=c++17 benchmarks/bdr_v6_adaptive_hybrid.cpp -o bdr_v6
./bdr_v6
```

Primary local experiment used:

- `N = 500,000`
- `M = 10,000`
- nominal global density `lambda = 50`
- `phi buckets = 65,536`
- 300,000 lookups per distribution
- deterministic RNG seed 42
- uniform and skewed/hot-key (Zipf-like) workloads

## Benchmark debugging note
The first hybrid prototype had a constructor bug: `th(th)` initialized the threshold member from itself instead of the constructor argument. This produced an invalid threshold and invalid partition-selection counts. Those measurements were discarded. The corrected form is `threshold(t)` / `th(threshold)`.

## Local occupancy
For the deterministic 500k-key dataset, occupancy varied around the global mean 50. Approximate counts observed during validation:

- threshold >= 32: ~9,964 / 10,000 partitions qualify for Robin Hood
- threshold >= 48: ~6,299 partitions qualify
- threshold >= 64: ~319 partitions qualify
- threshold >= 96: 0 partitions qualify
- maximum observed occupancy: ~79 records in a partition

This validates why a per-partition policy can differ materially from a global `lambda` rule.

## Repeated result around threshold 48
Threshold 48 was repeated five times because it was the most promising mixed point.

Median throughput from those repetitions:

| Engine | Uniform throughput | Zipf/hot-key throughput |
|---|---:|---:|
| Compact pure | ~2.91 Mops/s | ~2.55 Mops/s |
| Robin Hood pure | ~2.95 Mops/s | **~3.21 Mops/s** |
| Hybrid threshold=48 | **~3.11 Mops/s** | ~3.02 Mops/s |

Hybrid threshold=48 used approximately **47.5 bytes/record** in this implementation, versus roughly **21.4 bytes/record** for Compact pure and **55.1 bytes/record** for Robin Hood pure.

The hybrid therefore delivered the best median throughput for the uniform workload in these repetitions while using less memory than pure Robin Hood. Robin Hood pure remained the best for the tested hot-key workload.

## Tail latency caution
Per-query p99 sampling in this container showed occasional OS/scheduler outliers, including abnormally large p99 values in otherwise stable pure-engine runs. Throughput medians were substantially more stable. For this reason, tail-latency conclusions from this specific run are considered preliminary until repeated on pinned cores / a quieter host.

The hybrid threshold=48 typically produced p99 around ~1.1 us in the repeated local runs, but this value should not yet be treated as a production-grade latency guarantee.

## Engineering conclusion
A single global rule such as `if lambda >= 50 -> Robin Hood` is too coarse. Local `rho_R` occupancy is more informative.

A better policy is currently:

```text
for each rho_R partition:
    if local_occupancy >= threshold:
        use Robin Hood
    else:
        use Compact
```

Initial candidate threshold for this workload/hardware: **~48 records per partition**.

This threshold is empirical, not universal. It must be calibrated by workload and platform.

## Current best profiles
- **Memory-minimal / immutable-heavy:** Compact.
- **Hot-key / high-density / latency-sensitive:** Robin Hood.
- **Mixed uniform dataset with heterogeneous local density:** adaptive Hybrid is promising and achieved the best median throughput in this experiment.

## Next tests
1. Add writes and dynamic conversion Compact -> Robin Hood when a partition becomes hot or dense.
2. Add hysteresis to avoid repeated engine switching.
3. Compare against a conventionally sharded `unordered_map` with the same number of shards and locks.
4. Pin threads to cores and repeat p95/p99/p99.9 measurements.
5. Test larger datasets (1M, 10M where RAM permits).
6. Measure conversion cost and amortization time.

No claim of universally superior complexity or performance is made from this benchmark alone. Results establish a reproducible adaptive-storage hypothesis for further testing.
