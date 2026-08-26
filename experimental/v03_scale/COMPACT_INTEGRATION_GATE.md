# BDR v0.3 — Compact Index Integration Gate

Status: internal validation only. No publication, release candidate, DOI, or public milestone is intended before v1.0.

## Purpose

The compact-index prototype has shown materially lower resident memory in controlled ablations and has passed heavy-update churn with exact sampled-value verification. It must not replace `ResolutiveIndex` in the database core until it demonstrates full behavioral and persistence parity.

## Current evidence

- 1,000,000 records / 16-byte values: compact prototype materially reduced peak RSS versus the baseline index in CI.
- 100,000 records + 5,000,000 updates: compact GC path completed successfully.
- exact expected values are now verified on deterministic samples after churn.
- high-cardinality baseline remains memory-bound: 5,000,000 distinct records reached roughly 3 GiB peak RSS in CI.

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

The database currently replays BDR3 snapshots and BDW3 WAL records directly into `ResolutiveIndex`. A compact candidate must therefore pass the same end-to-end lifecycle:

1. create database;
2. insert/update/delete workload;
3. sync and checkpoint;
4. close cleanly;
5. reopen from BDR3 snapshot;
6. replay subsequent BDW3 WAL records;
7. verify complete final state against an external oracle;
8. repeat after simulated torn final WAL;
9. repeat after crash during active writes;
10. verify sequence/durability invariants remain unchanged.

## Resource gates

A compact implementation is useful only if the end-to-end database retains a meaningful advantage after persistence and recovery overhead are included.

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

The compact index may enter an experimental database integration only after full contract parity is green. It may replace the baseline index only when end-to-end correctness, crash recovery, concurrency, and resource gates are all green with reproducible evidence.

The target milestone is technical readiness for **v1.0**, not an intermediate v0.3 publication.
