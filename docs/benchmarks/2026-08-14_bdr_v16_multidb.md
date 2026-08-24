# BDR v16 — Multi-database durable write comparison

Date: 2026-08-14
Branch: `experiment/v0.2-cpp-encoder`
Environment: GitHub Actions `ubuntu-24.04`

## Purpose

Compare the BDR append-only WAL against established embedded database engines under explicit durable-write settings, using one C++ benchmark process and the same key/value workload.

Engines installed by the CI runner:

- SQLite 3.45.1 (`journal_mode=WAL`, `synchronous=FULL`)
- LMDB 0.9.31 (default durable transaction commit)
- LevelDB 1.23 (`WriteOptions.sync=true`)
- RocksDB 8.9.1 (`WriteOptions.sync=true`)
- BDR minimal WAL (`fdatasync()` at each batch boundary)

Workload: 5,000 PUTs per engine, batch sizes 1, 32, and 128, three independent repetitions. Values below are medians of the three repetitions.

## Durable PUT results

| Batch | BDR WAL | SQLite | LMDB | LevelDB | RocksDB |
|---:|---:|---:|---:|---:|---:|
| 1 | 1,785 ops/s | **4,468 ops/s** | 2,814 ops/s | 2,560 ops/s | 2,448 ops/s |
| 32 | 64,667 ops/s | 64,245 ops/s | **84,881 ops/s** | 53,357 ops/s | 54,673 ops/s |
| 128 | 162,384 ops/s | 212,456 ops/s | 198,005 ops/s | **225,234 ops/s** | 188,644 ops/s |

## Interpretation

### Batch = 1

BDR is the slowest engine in this run. SQLite is approximately 2.5x faster than the minimal BDR WAL for a durability barrier at every operation. Therefore the project must not claim that BDR currently dominates established engines for single-operation durable commits.

### Batch = 32

BDR becomes competitive after amortizing the durability barrier. It is approximately equal to SQLite in this run, faster than LevelDB and RocksDB, but slower than LMDB.

### Batch = 128

BDR improves substantially but is still behind all four established engines in this run. LevelDB is the fastest median result at this batch size.

The crossover demonstrates that group commit is important, but the current BDR WAL implementation is not yet uniformly superior to mature embedded databases.

## Important invalid GET measurement

The first v16 benchmark accidentally measured the BDR read side by iterating an already-materialized in-memory vector, while SQLite/LMDB/LevelDB/RocksDB executed actual key lookups. This produced impossible-looking BDR values near 10^9 ops/s.

Those BDR GET numbers are **invalid and must not be cited**. They are deliberately excluded from the table above. A corrected read benchmark must use the same lookup semantics for every engine, including an actual BDR index lookup using the selected `rho/fingerprint` or `rho/phi/fingerprint` address variant.

## Scientific conclusion

The durable-write comparison does not show a universal BDR advantage. Instead it identifies a workload-dependent profile:

- single durable commit: mature engines currently win;
- moderate group commit (32): BDR is competitive and second to LMDB in this run;
- larger group commit (128): mature engines again lead the current BDR WAL.

This negative/mixed result is retained as part of the reproducible v0.2 experimental record.

## Reproduction

Source: `benchmarks/bdr_v16_multi_db_compare.cpp`
Workflow: `.github/workflows/v02-multidb-benchmark.yml`
GitHub Actions run: `31834202260`
Artifact: `bdr-v16-multidb-results`
