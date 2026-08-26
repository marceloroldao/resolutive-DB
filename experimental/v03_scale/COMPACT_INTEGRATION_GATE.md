# BDR v0.3 — Compact Index Integration Gate

Status: internal validation only. No publication, release candidate, DOI, or public milestone is intended before v1.0.

## Purpose

The compact-index prototype has shown materially lower resident memory in controlled ablations and has passed heavy-update churn with exact sampled-value verification. It must not replace `ResolutiveIndex` in the database core until it demonstrates full behavioral and persistence parity.

## Current evidence

- contract parity: PASS against baseline + external oracle;
- churn + reclaim: PASS with exact expected-value verification;
- BDR3/BDW3 persistence parity: PASS, including torn final WAL and CRC rejection;
- real Database lifecycle with compact backend: PASS;
- crash recovery: PASS (50 kill/recovery rounds);
- concurrency: PASS with 1/4/8/16 writers;
- paired same-runner 1M: compact median RSS 231,244 KB vs baseline 299,632 KB (-22.8%), throughput effectively equivalent;
- paired same-runner 5M non-streaming: compact median RSS 970,876 KB vs baseline 1,325,728 KB (-26.8%), insert throughput +1.67%;
- streaming BDR3 encoder: byte-identical snapshot, reducing checkpoint ablation RSS by ~14.6%;
- combined compact + streaming at 1M: lifecycle, crash and concurrency PASS;
- paired same-runner 5M compact + streaming lifecycle: PASS.

### Paired 5M compact + streaming medians

| Metric | baseline + streaming | compact + streaming | compact relative to baseline |
|---|---:|---:|---:|
| insert throughput | 331,914 ops/s | 434,487 ops/s | 1.309x |
| checkpoint time | 19.9069 s | 19.1472 s | 0.962x |
| reopen time | 2.16336 s | 1.71578 s | 0.793x |
| lookup throughput | 1,772,140 ops/s | 2,100,340 ops/s | 1.185x |
| peak RSS | 1,955,716 KB | 1,480,200 KB | 0.757x (-24.3%) |
| disk bytes | 277,777,840 | 277,777,840 | identical |

The paired lifecycle shows that compact + streaming retains a material memory advantage at 5M records while preserving the BDR3 disk footprint. Insert timing is noisy across repetitions, so the throughput improvement is supporting evidence rather than a correctness requirement; the persistent advantages are lower RSS and faster reopen.

## Mandatory contract parity

The compact implementation must provide and pass equivalent semantics for every index operation used by `Database`:

- `put(key, value)`
- `get(key)`
- `erase(key)`
- `contains(key)`
- `size()`
- `stats()`
- `snapshot_items()`

No core integration is allowed while any of these operations is missing or only partially validated.

## Mandatory correctness gates

1. **Insert parity** — same final key/value state as the baseline index.
2. **Overwrite parity** — deterministic final values after repeated updates.
3. **Delete parity** — deleted keys absent; reinserted keys correct.
4. **Binary-data parity** — keys/values with embedded zero bytes and non-text bytes remain exact.
5. **Snapshot parity** — canonicalized `snapshot_items()` output matches baseline state.
6. **Statistics sanity** — record count and load-related statistics remain internally consistent.
7. **Concurrent-reader safety** — readers never observe malformed key/value data during concurrent writes.
8. **Concurrent-writer correctness** — final state matches an external deterministic oracle.

## Mandatory persistence gates

The database replays BDR3 snapshots and BDW3 WAL records directly into the index. A compact candidate must therefore pass the same end-to-end lifecycle:

1. create database;
2. insert/update/delete workload;
3. sync and checkpoint;
4. close cleanly;
5. reopen from BDR3 snapshot;
6. replay subsequent BDW3 WAL records;
7. verify complete final state against an external oracle;
8. repeat after simulated torn final WAL;
9. repeat after crash during active writes;
10. verify sequence/durability invariants remain unchanged;
11. crash at deterministic boundaries inside streaming checkpoint construction and WAL rotation, then recover exactly.

## Checkpoint crash-boundary gate

The active gate kills the checkpointing process with SIGKILL at twelve deterministic boundaries:

- middle of snapshot.tmp write;
- after snapshot write;
- after snapshot fsync;
- after snapshot close;
- after rename to snapshot.bdr3;
- after snapshot directory fsync;
- after old WAL close;
- after new WAL creation;
- after new-WAL directory fsync;
- before old-WAL removal;
- after old-WAL removal;
- after old-WAL-removal directory fsync.

For every boundary and both baseline/compact streaming backends, recovery must return the complete durable state (65,000 operations, 45,000 final records), preserve deletes, and preserve the durable sequence.

## Resource gates

Measure at minimum:

- peak RSS at 100k / 1M / 5M distinct records;
- bytes per record in memory;
- insert throughput;
- lookup throughput;
- overwrite throughput;
- delete/reinsert throughput;
- checkpoint latency;
- reopen/recovery latency;
- arena live bytes;
- garbage bytes;
- compaction count and compaction CPU cost.

## Promotion rule

The compact index may enter an experimental database integration only after full contract parity is green. It may become part of the internal v1 candidate only when end-to-end correctness, crash recovery, concurrency, resource gates, and checkpoint crash-boundary recovery are all green with reproducible evidence.

The target milestone is technical readiness for **v1.0**, not an intermediate v0.3 publication.
