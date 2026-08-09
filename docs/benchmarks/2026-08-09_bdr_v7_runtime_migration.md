# BDR V7 — Runtime Compact→Robin Hood migration

Date: 2026-08-09

## Objective

Test whether a BDR partition can begin in Compact mode and migrate at runtime to Robin Hood when occupancy or observed write ratio justifies the conversion.

## Reproduction

```bash
g++ -O3 -std=c++17 benchmarks/bdr_v7_runtime_migration.cpp -o bdr_v7_runtime
./bdr_v7_runtime
```

Parameters used in the recorded run:

- total key universe: N = 500,000
- initially materialized records: 200,000
- rho partitions: M = 10,000
- phase buckets: 65,536
- occupancy thresholds tested: 48 and 64
- write-ratio thresholds tested: 10% and 25%
- 150k read operations before migration stage
- 300k operations in each mixed stage

## Recorded results

| occupancy threshold | write trigger | stage | Mops/s | p99 us | migrations | cumulative migration ms | Robin partitions | approx bytes/record* |
|---:|---:|---|---:|---:|---:|---:|---:|---:|
| 48 | 0.10 | read_before | 2.332 | 0.834 | 0 | 0 | 0 | 27.29 |
| 48 | 0.10 | mixed_migrate | 2.618 | 1.001 | 2,064 | 8.180 | 2,064 | 14.21 |
| 48 | 0.10 | mixed_after | 2.734 | 1.084 | 4,239 | 15.002 | 4,239 | 17.30 |
| 48 | 0.25 | read_before | 3.628 | 0.785 | 0 | 0 | 0 | 27.29 |
| 48 | 0.25 | mixed_migrate | 3.109 | 1.348 | 3 | 0.0035 | 3 | 11.46 |
| 48 | 0.25 | mixed_after | 3.092 | 0.826 | 8 | 0.0084 | 8 | 11.96 |
| 64 | 0.10 | read_before | 2.857 | 0.731 | 0 | 0 | 0 | 27.29 |
| 64 | 0.10 | mixed_migrate | 1.521 | 0.911 | 2,064 | 2.896 | 2,064 | 14.21 |
| 64 | 0.10 | mixed_after | 2.769 | 1.199 | 4,239 | 8.253 | 4,239 | 17.30 |
| 64 | 0.25 | read_before | 3.636 | 0.767 | 0 | 0 | 0 | 27.29 |
| 64 | 0.25 | mixed_migrate | 3.180 | 0.827 | 3 | 0.0029 | 3 | 11.46 |
| 64 | 0.25 | mixed_after | 2.782 | 0.855 | 8 | 0.0077 | 8 | 11.96 |

\* Memory accounting is an implementation estimate and the denominator changes with the staged experiment; it is useful for relative comparison, not as a final allocator-level RSS measurement.

## Interpretation

The runtime conversion mechanism itself is cheap for a single small partition. The decisive issue is policy, not the conversion algorithm.

A write-ratio trigger of 10% is too aggressive for this workload. It causes thousands of partitions to migrate even when the measured benefit does not justify converting so much of the index. A 25% trigger is substantially more selective: only a handful of partitions migrated and throughput remained higher in the mixed workload.

This experiment therefore rejects the naive policy `write_ratio >= 0.10 => migrate` as a default rule.

## Next policy revision

The next runtime policy should use hysteresis and a minimum evidence window. Candidate policy:

```text
migrate to Robin Hood only if
    occupancy >= density_threshold
OR
    operations_observed >= W
    AND write_ratio >= write_threshold
    AND estimated_benefit > estimated_migration_cost
```

The migration decision should be one-way in v0.1 to avoid oscillation. Compact→Robin conversion can later be complemented by offline compaction back to Compact.

## Release status

This result is not sufficient for a v0.1 release by itself. Persistence, restart integrity, deletion/update semantics, crash recovery and stronger external baselines remain required before declaring a first usable database version.
