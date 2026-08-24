# BDR v14 — Durable Group Commit: C++ vs SQLite C API

**Date:** 2026-08-14  
**Branch:** `experiment/v0.2-cpp-encoder`  
**Status:** experimental / reproducible benchmark

## Goal

Measure BDR append-only WAL group commit against SQLite using comparable durable transaction boundaries, while removing the Python-driver confounder from earlier experiments.

## Method

- Both engines implemented/called from the same C++17 benchmark process.
- BDR uses one persistent POSIX file descriptor, `writev()` per logical record, and `fdatasync()` once per group.
- SQLite uses the native C API with:
  - `PRAGMA journal_mode=WAL;`
  - `PRAGMA synchronous=FULL;`
  - `PRAGMA wal_autocheckpoint=0;`
  - one explicit transaction per group.
- Dataset: 8,192 inserts with deterministic string keys and payloads.
- Group sizes: 1, 8, 16, 32, 64, 128.
- Three complete independent executions were used. The table below reports the median across runs.
- `batch_p50` and `batch_p99` are transaction/barrier latencies, not independent per-record durable latency. `amort_us_op` is total wall time divided by logical operations.

## Median results across 3 runs

| Engine | Batch | Throughput (ops/s) | Amortized us/op | Batch p50 (us) | Batch p99 (us) |
|---|---:|---:|---:|---:|---:|
| BDR | 1 | 1,610.85 | 620.790 | 483.120 | 2,086.260 |
| SQLite | 1 | 1,380.17 | 724.548 | 599.844 | 3,031.610 |
| BDR | 8 | 14,793.90 | 67.596 | 469.019 | 1,377.910 |
| SQLite | 8 | 10,623.90 | 94.127 | 626.709 | 3,900.880 |
| BDR | 16 | 25,611.40 | 39.045 | 539.203 | 1,801.030 |
| SQLite | 16 | 23,600.50 | 42.372 | 595.077 | 1,742.410 |
| BDR | 32 | 57,427.90 | 17.413 | 486.139 | 1,359.500 |
| SQLite | 32 | 53,212.90 | 18.793 | 540.175 | 1,470.730 |
| BDR | 64 | 105,259.00 | 9.500 | 508.317 | 1,546.660 |
| SQLite | 64 | 75,342.70 | 13.273 | 768.550 | 2,525.120 |
| BDR | 128 | 190,248.00 | 5.256 | 638.331 | 1,017.580 |
| SQLite | 128 | 138,499.00 | 7.220 | 843.492 | 4,088.730 |

## Relative throughput in this environment

Approximate BDR / SQLite ratios from the median runs:

- batch 1: **1.17x**
- batch 8: **1.39x**
- batch 16: **1.09x**
- batch 32: **1.08x**
- batch 64: **1.40x**
- batch 128: **1.37x**

## Interpretation

This experiment corrects an important methodological issue in earlier C++ persistence measurements: `std::ofstream::flush()` is not equivalent to a durable storage barrier. The v14 benchmark uses `fdatasync()` for BDR and `synchronous=FULL` transaction commits for SQLite.

On this machine and filesystem, BDR achieved higher median throughput than SQLite at every tested group size. This is evidence about this minimal append-only key-value WAL path under this workload, not a general claim that BDR is faster than SQLite as a database system.

SQLite performs substantially more database work: transactional page management, B-tree maintenance, SQL engine semantics, schema management, and its own WAL protocol. BDR v14 is intentionally a narrow storage-engine benchmark. The result therefore identifies a promising performance envelope for a specialized BDR persistence layer, not equivalence of feature sets.

## Important limitations

1. Hardware, kernel, filesystem and storage cache behavior strongly affect sync latency.
2. Only inserts were measured in this benchmark.
3. BDR's record format here is a benchmark format, not a complete production transaction protocol.
4. No concurrent writers were used.
5. p99 for large batches is based on fewer transactions than p99 for batch 1.
6. Power-loss guarantees ultimately depend on filesystem/device behavior beyond process-level API semantics.
7. This benchmark does **not** validate the resolutive encoder (`rho_R`, `phi`, `theta`, `f_nu`); persistence and addressing remain separate experimental questions.

## Reproduction

Example build on a Linux system with SQLite development headers:

```bash
g++ -O3 -std=c++17 benchmarks/bdr_v14_group_commit_cpp_vs_sqlite.cpp -lsqlite3 -o bdr_v14
./bdr_v14
```

For scientific reporting, run the executable multiple times and preserve raw CSV output together with OS, CPU, storage device, filesystem, compiler and SQLite version.

## Scientific conclusion

The corrected result is stronger and narrower than the earlier `flush()` comparison:

> A minimal BDR append-only WAL with a persistent file descriptor and real `fdatasync()` group commits was competitive with, and in these three runs faster than, SQLite WAL/FULL transactions at matched group boundaries. This does not establish general database superiority and does not validate the resolutive addressing mechanism.
