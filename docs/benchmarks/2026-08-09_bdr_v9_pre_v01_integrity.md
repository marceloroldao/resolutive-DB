# BDR V9 — Pre-v0.1 integrity, durability and SQLite comparison

Date: 2026-08-09

## Scope

This battery tests segmented WAL sequencing, differential fuzzing, truncation/corruption handling, fsync durability policy, SQLite persistence baselines, and atomic checkpoint behavior.

## Differential fuzz

Five deterministic seeds were executed with 200,000 random PUT/DELETE operations each (1,000,000 operations total) against a Python dict reference model.

Result: all five runs matched the reference exactly. Every valid run ended at sequence 200,000 with zero gaps and zero checksum failures.

## Truncated WAL tail

After 10,000 PUTs, 13 bytes were removed from the final WAL segment.

Observed: 9,999 records replayed; one incomplete/corrupt record detected; zero sequence gaps among accepted records. The incomplete final operation was not applied.

## Mid-segment corruption

A byte was deliberately flipped in a middle segment. CRC detected one corrupt record. The current experimental replayer continued with later segments and therefore reported a sequence gap. This is intentionally treated as a limitation: production recovery must quarantine the damaged segment/range and require an explicit policy rather than silently accepting a discontinuous log.

## Durability / fsync comparison

Environment-specific measurements, 2,000 writes:

| Mode | BDR | SQLite |
|---|---:|---:|
| durability barrier each operation | 1.466 s | 0.875 s |
| group commit, 100 operations | 0.0348 s | 0.0259 s |

SQLite uses WAL mode and `synchronous=FULL`. Results are local measurements, not universal performance claims. The key conclusion is that per-operation fsync is expensive for the current BDR implementation and group commit materially reduces the cost. SQLite remains faster in this test.

A separate 50,000-write NORMAL-mode comparison produced broadly similar insert time (BDR ~0.091 s, SQLite ~0.086 s), but SQLite reopen/count was much faster because BDR replayed the WAL while SQLite reopened an indexed persistent structure. This reinforces the need for regular checkpoints/index persistence.

## Atomic checkpoint crash test

Snapshot write protocol:

1. write a temporary snapshot;
2. fsync the temporary file;
3. atomic rename/replace to canonical snapshot;
4. fsync the parent directory.

Simulated crash before rename preserved the previous canonical snapshot exactly. A corrupt orphan `.tmp` file did not affect canonical recovery. After successful rename, the new snapshot reopened exactly.

## Pre-v0.1 assessment

Positive:

- 1,000,000 differential fuzz operations with no logical divergence;
- deterministic sequence numbers across segmented WAL;
- tail truncation detection;
- checksum corruption detection;
- atomic checkpoint protocol behaves safely in the tested pre-rename crash scenario;
- group commit provides practical durability throughput.

Still required before declaring v0.1 experimental:

- define strict recovery semantics for corruption inside an earlier WAL segment (no implicit sequence gaps);
- integrate snapshot + segmented WAL into one engine rather than isolated benchmark components;
- test process kill/power-loss style failure at multiple checkpoint/WAL stages;
- persist and rebuild the adaptive rho_R index, including update/delete semantics;
- multi-writer persistent concurrency and ordering tests;
- larger long-duration fuzz/soak tests;
- end-to-end benchmark against SQLite using the integrated engine and matched transaction boundaries.

The BDR is not yet marked v0.1 by this report, but the storage layer has passed the first integrity gate.
