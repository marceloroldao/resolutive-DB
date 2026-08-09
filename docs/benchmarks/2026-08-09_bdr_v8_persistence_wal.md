# BDR V8 — Persistence, Reopen and Torn-WAL Recovery

Date: 2026-08-09

## Objective

Move BDR from an in-memory indexing experiment toward database semantics by validating:

- persistent writes;
- deterministic reopen/replay;
- update semantics;
- delete semantics;
- detection of an incomplete final WAL record;
- preservation of the complete valid prefix after a torn write.

## Prototype format

The experiment uses an append-only binary WAL. Each record contains:

- magic/version marker;
- operation (`PUT` or `DELETE`);
- key length;
- value length;
- 64-bit checksum;
- key bytes;
- value bytes.

This is intentionally a minimal audit-friendly format. It is **not yet a production durability layer**: there is no transaction grouping, fsync policy, snapshot compaction, WAL rotation, sequence number, or checksum tree.

## Reproduction

Compile:

```bash
g++ -O3 -std=c++17 benchmarks/bdr_v8_persistence_wal.cpp -o bdr_v8_persistence
```

Run in a disposable directory:

```bash
./bdr_v8_persistence
```

The program creates `bdr_v8_test.wal`, writes the workload, reopens it, then deliberately truncates 17 bytes from the tail and reopens again.

## Workload executed in this session

- 100,000 initial inserts;
- 10,000 updates of existing keys;
- 5,000 deletes;
- total valid WAL operations before truncation: 115,000;
- expected live records after all complete operations: 95,000.

## Observed result

```text
phase=write,size=95000
phase=reopen,size=95000,good=115000,bad=0,checks=3,load_s=0.0495884
phase=truncated_reopen,size=95001,good=114999,bad=1,checks=2
```

WAL size after truncation in this environment: approximately 3.6 MiB.

## Interpretation

### Clean reopen

The clean replay reconstructed exactly 95,000 live records. Three semantic checks passed:

1. an updated record reopened with its updated value;
2. a deleted record remained absent;
3. an untouched record reopened with its original value.

Replay of 115,000 WAL operations took approximately 0.050 seconds in this environment.

### Torn final record

After deliberately removing 17 bytes from the WAL tail, the loader:

- replayed 114,999 complete records;
- detected one incomplete/corrupt record;
- stopped at the damaged tail;
- retained the full valid prefix.

The resulting live count was 95,001 instead of 95,000 because the final operation was a delete whose record was deliberately torn. Ignoring an incomplete final operation leaves the database in the state immediately before that operation, which is the intended recovery behavior for this minimal WAL model.

## What this test proves

This experiment demonstrates a basic prefix-recovery property:

> a truncated final WAL record is not accepted as committed data, while all complete preceding records remain recoverable.

It also validates persistent PUT/UPDATE/DELETE replay for this prototype.

## What it does not prove

This test does **not** yet establish production durability or ACID semantics. Missing pieces include:

- `fsync` / `fdatasync` durability guarantees;
- atomic multi-record transactions;
- monotonic sequence numbers;
- WAL rotation;
- snapshots/checkpoints;
- crash during checkpoint replacement;
- corruption in the middle of the WAL;
- checksum stronger than the current FNV-derived prototype checksum;
- protection against maliciously large length fields;
- concurrent writers;
- endian/version portability;
- disk-full and I/O-error handling.

## Next tests

1. Add sequence numbers and bounded record sizes.
2. Add snapshot/checkpoint plus WAL replay after snapshot.
3. Test corruption in the middle of the WAL, not just the tail.
4. Test repeated crash/restart cycles.
5. Add update/delete fuzz testing against a reference map.
6. Add durable sync modes (`none`, `batch`, `per-commit`) and measure throughput.
7. Compare persistent point lookup/write workloads against SQLite under equivalent durability settings.
