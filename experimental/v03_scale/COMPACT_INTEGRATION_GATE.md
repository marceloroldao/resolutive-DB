# BDR v0.3 — Compact Index Integration Gate

Status: internal validation only. No publication, release candidate, DOI, or public milestone is intended before v1.0.

## Purpose

The compact-index candidate reduces resident memory while preserving the existing database contract. It must remain experimental until the combined backend + persistence + crash + checkpoint path is reproducibly green.

## Current evidence

- contract parity against `ResolutiveIndex` plus external oracle: PASS;
- binary keys/values, overwrite, delete/reinsert, statistics and canonical snapshot parity: PASS;
- 100,000 records + 5,000,000 updates with exact sampled final-value verification: PASS;
- BDR3 snapshot + BDW3 WAL replay parity, torn-final-WAL handling and CRC rejection: PASS;
- real `Database` lifecycle through compact backend shim: PASS;
- crash recovery: PASS across 50 kill/recovery rounds;
- concurrency: PASS for 1 / 4 / 8 / 16 writers;
- compile-time backend selector: baseline PASS and compact PASS, with baseline remaining the default path;
- paired same-runner 1M median: compact 684,899 ops/s vs baseline 682,411 ops/s; compact RSS 231,244 KB vs baseline 299,632 KB (-22.8%);
- paired same-runner 5M median: compact 667,267 ops/s vs baseline 656,275 ops/s; compact RSS 970,876 KB vs baseline 1,325,728 KB (-26.8%);
- compact memory advantage increases with cardinality through 5M while ingest throughput remains at least equivalent.

## Mandatory contract parity

The compact implementation must provide and preserve equivalent semantics for every index operation used by `Database`:

- `put(key, value)`
- `get(key)`
- `erase(key)`
- `contains(key)`
- `size()`
- `stats()`
- `snapshot_items()`

These contract gates are currently green and remain mandatory regression checks.

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

The compact candidate must preserve the same end-to-end lifecycle:

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

These gates are green for the compact backend and remain mandatory regressions.

## Checkpoint memory finding

The current checkpoint path independently inflates RSS because it materializes both `snapshot_items()` and a second complete encoded BDR3 byte vector.

A streaming BDR3 encoder that preserves the current sorted snapshot order has been validated at 1M records:

- output size: 53,777,808 bytes for both buffered and streaming variants;
- byte-for-byte equality: PASS (`cmp`);
- SHA-256 for both: `890cc814527e6879e0f0594f72a355d237c7afc95d82001a6d2e7fe4c4d629e7`;
- buffered RSS: 380,224 KB;
- streaming RSS: 324,644 KB (~14.6% lower);
- streaming carries a small runtime cost and therefore remains experimental.

The next gate is the combined real-Database path: baseline/compact backend × streaming checkpoint, preserving BDR3 atomicity, crash recovery and concurrency.

## Resource gates

Continue measuring:

- peak RSS at 100k / 1M / 5M distinct records;
- insert and lookup throughput;
- overwrite and delete/reinsert throughput;
- checkpoint latency and RSS;
- reopen/recovery latency and RSS;
- disk footprint;
- arena live/garbage bytes;
- compaction count and CPU cost.

## Promotion rule

The compact backend may continue in controlled experimental integration because contract, persistence, crash, concurrency and paired high-cardinality resource evidence are green through 5M records.

It must not become the default backend until the combined compact + streaming path and later v1 readiness gates remain reproducibly green. The current baseline remains the default throughout pre-v1 development.

The target milestone is technical readiness for **v1.0**, not an intermediate v0.3 publication.
