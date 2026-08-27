# BDR v1.1.0 — Release Notes

Status: **publication ready / tag pending**.

BDR v1.1.0 extends the stable v1 line with an opt-in atomic logical-write path designed around direct Memoria.ia persistence requirements. The published `v1.0.0` baseline remains immutable and existing `bdr::Database` source calls remain supported.

## Highlights

- Additive public `bdr::AtomicDatabase` API through `bdr/atomic_database.hpp`.
- Atomic `write_batch` with one logical commit sequence for multiple physical records.
- Native `put_many` and `erase_many` convenience paths.
- Explicit durability modes: `Async`, `BatchSync`, `PerOperationSync`.
- New explicitly versioned BDW4 atomic WAL framing with complete-frame CRC.
- Side-by-side recovery from existing BDR3 snapshots and BDW3 WAL segments without rewriting the v1.0 files.
- Torn final BDW4 batch recovery to the last complete valid boundary.
- Installed CMake package continues to use the single public target `bdr::bdr`.

## Compatibility

Existing v1.0 consumers may continue using:

```cpp
#include <bdr/database.hpp>
```

without source changes. Applications that need logical all-or-nothing batches opt into:

```cpp
#include <bdr/atomic_database.hpp>
```

BDR3 and BDW3 are not reinterpreted. New atomic writes are stored separately in BDW4 format.

## Durability semantics

- `Async`: the complete batch is visible after append, but durability is not claimed until `sync()`.
- `BatchSync`: the complete atomic batch crosses a durable flush boundary before returning a durable result.
- `PerOperationSync`: accepted only for exactly one operation.

`last_sequence()` reports the latest complete visible batch. `durable_sequence()` reports the latest known durable boundary for the current process.

## Validation summary

The v1.1 candidate passed:

- V101 Atomic Batch;
- V102 File WAL;
- V103 Commit Boundary;
- V104 Batch API;
- V105 Concurrency;
- V106 Migration;
- V107 Integrated Candidate;
- V108 Integrated Stress;
- V109 Integrated Crash Recovery;
- V110 Durability Contract;
- V111 Public API Compatibility;
- V112 Memoria Atomic Benchmark;
- BDR CI;
- V99 Evidence Closure;
- V100 Evidence Closure;
- final post-merge BDR CI on `main`.

V109 exercises every truncation position of a representative next batch after a durable prefix and verifies all-or-none recovery while preserving legacy v1.0 files.

V111 installs the library into a clean prefix and builds/runs an external `find_package(bdr)` consumer that uses both the frozen v1.0 API and the additive v1.1 atomic API.

## Representative Memoria.ia workload

V112 uses 512 logical memories with 24 physical records each (12,288 records total), with one durability boundary per logical memory.

After V113 removed quadratic full-state copies from BDW4 replay, the recorded same-run comparison was:

| Metric | v1.0 write cadence | v1.1 atomic | Result |
|---|---:|---:|---:|
| Write | 4,918.203 ms | 1,696.555 ms | v1.1 ~2.90× faster |
| Reopen + full verify | 29.120 ms | 13.851 ms | v1.1 ~2.10× faster |
| Disk footprint | 3,829,120 B | 3,597,664 B | v1.1 smaller |

These measurements are runner- and workload-specific regression evidence, not universal performance claims.

## Important non-goals

v1.1.0 does not introduce general SQL-style transactions, MVCC, distributed transactions, cross-platform I/O abstraction, checkpoint telemetry or a new Python binding contract. Those belong to later v1.x work.

## Publication status

The technical v1.1.0 tree is merged and validated. Publication metadata is staged without inventing a DOI. The `v1.1.0` tag and GitHub Release are created only after this finalization branch passes its release audits. The definitive software DOI is recorded only after the publication service returns the actual identifier.
