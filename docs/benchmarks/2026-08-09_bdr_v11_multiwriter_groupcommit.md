# BDR V11 — Multiwriter, Group Commit, WAL Retirement, and Soak

Date: 2026-08-09
Status: pre-v0.1 gate

## Scope

This gate validates the integrated persistent engine under concurrent writers and repeated checkpoint/reopen cycles. It also introduces explicit snapshot format versioning, snapshot CRC validation, configurable group commit, and safe retirement of WAL segments after an atomic checkpoint.

## Implemented engine changes

- `threading.RLock` serializes state/WAL mutation in the Python reference engine.
- `group_commit_ops` controls automatic fsync batching.
- `durable=True` forces an fsync before the operation returns.
- V2 snapshot envelope (`BDR2`) contains a formal format version and a CRC32 over the entire snapshot payload.
- V1 snapshots (`BDR1`) remain readable for backward compatibility.
- Checkpoint sequence: fsync active WAL -> write+fsync `snapshot.tmp` -> atomic replace -> fsync directory -> open a new WAL -> retire old WAL files -> fsync directory.
- Recovery continues to reject CRC failures, unknown opcodes, and sequence gaps.

## Measured local results

### Multiwriter prototype

- 8 writer threads
- 10,000 operations/thread
- 80,000 total PUT/DELETE operations
- elapsed: ~2.904 s
- checkpoint + reopen state equality: PASS
- final sequence: 80,000
- WAL files after checkpoint/reopen: one active segment

### Group commit timing (5,000 writes)

| fsync grouping | elapsed |
|---:|---:|
| 1 (per operation) | ~3.914 s |
| 8 | ~0.590 s |
| 32 | ~0.194 s |
| 64 | ~0.113 s |
| 128 | ~0.069 s |

These measurements show the expected durability/throughput trade-off. They do not imply that a larger group is always preferable: larger groups increase the number of acknowledged-but-not-yet-fsynced operations unless callers request `durable=True`.

### Soak

- 500,000 randomized PUT/DELETE operations
- 50,000-key working set
- checkpoint + close + reopen every 100,000 operations
- five reopen comparisons against an independent in-memory reference
- final live records: 39,019
- elapsed: ~11.656 s
- state divergence: 0

## Important limitations

The current Python reference engine uses a process-local lock. It validates correctness and persistence semantics, not maximum parallel write throughput. The compiled BDR partitioned index remains the intended path for scalable concurrency.

A real external `SIGKILL`/power-loss campaign remains pending because the current execution environment blocked the separate worker process used for that test. Torn-tail, CRC corruption, atomic checkpoint interruption, and sequence-gap recovery have already been tested independently.

## Gate assessment

PASS for:

- integrated multiwriter correctness under threads;
- group commit behavior;
- checkpoint/reopen soak;
- snapshot format versioning and checksum;
- WAL retirement policy design;
- explicit durable write API.

Still required before BDR v0.1.0 Research Preview:

1. run the updated repository test suite in CI/a normal host;
2. external process kill/power-loss injection at randomized durability points;
3. end-to-end comparison with SQLite using identical transaction boundaries after the V11 engine changes;
4. package/release metadata and documented on-disk format contract.
