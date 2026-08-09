# BDR V10 — Integrated Persistent Engine Validation

Date: 2026-08-09

## Scope

This stage integrates the previously isolated persistence mechanisms into one prototype engine with a single API:

- `put(key, value)`
- `get(key)`
- `delete(key)`
- `checkpoint()`
- close / reopen / recovery

The persistence layer uses:

1. segmented append-only WAL;
2. monotonic sequence numbers;
3. CRC32 per WAL record;
4. atomic snapshot/checkpoint using temporary file + fsync + rename + directory fsync;
5. replay from snapshot sequence forward.

This remains an experimental prototype and is not yet a production database.

## Differential fuzz

Five deterministic seeds were executed locally. Each seed performed 50,000 randomly generated PUT/DELETE operations against both the integrated BDR prototype and a Python reference dictionary, with intermediate checkpoints.

Total tested operations: **250,000**.

Result: **zero final-state divergences** after close and reopen for all five seeds.

## End-to-end persistence comparison with SQLite

A deterministic workload of 100,000 operations was executed against the integrated BDR prototype and SQLite. The workload used approximately 75% PUT/update and 25% DELETE across a 30,000-key space.

Measured in the current execution environment:

| Metric | Integrated BDR | SQLite |
|---|---:|---:|
| 100k operation workload + final checkpoint/transaction | ~0.195 s | ~0.242 s |
| BDR reopen | ~0.092 s | not measured in this particular run |
| Final live records | 21,776 | 21,776 |
| Final state equality | yes | yes |

Important: this benchmark is **not** a claim that BDR is generally faster than SQLite. The persistence semantics and transaction boundaries here are deliberately simple. SQLite has a mature transactional engine, query planner, B-tree storage, locking, recovery, and many features absent from this prototype.

## Torn-tail recovery

The final WAL was deliberately truncated. The loader accepts only complete records. A torn final record is ignored, while the valid prefix remains recoverable.

## Sequence-gap policy

Unlike earlier exploratory WAL tests, the integrated loader treats a discontinuity in sequence numbers as a `RecoveryError`. It must not silently skip a missing durable operation and continue replay as if the history were continuous.

## Real process-kill status

An attempt was made to execute a separate Python worker and kill it at random points. The execution environment blocked the worker during Python startup because of an unrelated runtime warmup mechanism. Therefore **real external process-kill crash testing is still pending and is not claimed as passed**.

Controlled torn-write, checksum corruption, and atomic-checkpoint tests from earlier stages remain valid, but an external `SIGKILL`/power-loss style campaign must be run in CI or on a normal host before v0.1 readiness is declared.

## Current assessment

The integrated persistence design has now demonstrated:

- deterministic recovery;
- PUT/update/delete replay;
- atomic checkpointing;
- torn-tail handling;
- strict sequence-gap detection;
- differential correctness across randomized workloads;
- equality with a SQLite reference state for an end-to-end workload.

Remaining v0.1 gates include:

1. external process-kill crash campaign;
2. multiwriter persistent concurrency;
3. group-commit integration in the main engine;
4. WAL rotation/retirement after checkpoint;
5. longer soak testing and larger datasets;
6. packaging/API stabilization and documented on-disk format versioning.
