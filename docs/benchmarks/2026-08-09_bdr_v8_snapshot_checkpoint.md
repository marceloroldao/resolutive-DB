# BDR V8 — Snapshot/Checkpoint + Incremental WAL

Date: 2026-08-09

## Objective

Reduce restart cost compared with replaying an unbounded WAL, and test integrity under mid-WAL corruption.

## Method

1. Build 200,000 live records in memory.
2. Write a checksummed binary snapshot to a temporary file, `fsync`, then atomically replace the snapshot path.
3. Reset the WAL.
4. Append 10,000 updates and 5,000 deletes to the WAL.
5. Reopen by loading snapshot then replaying WAL.
6. Compare the full recovered database against the expected in-memory state.
7. Corrupt one byte near the middle of the WAL and replay again.

## Reproduction

```bash
python benchmarks/bdr_v8_snapshot_checkpoint.py
```

## Results observed in this session

```text
snapshot_write_s 0.1773
snapshot_load_s 0.3027
wal_replay_s 0.0244
wal_good 15000
wal_bad 0
live 195000
exact_match True
```

Approximate file sizes:

- snapshot: 4.75 MiB;
- incremental WAL: 0.42 MiB.

After deliberate mid-WAL corruption:

```text
wal_good 7213
wal_bad 1
prefix_preserved True
```

The checksum detected the corrupted WAL record and replay stopped at the first invalid record.

## Interpretation

The checkpoint design successfully reconstructed the exact expected state after 200,000 snapshot records plus 15,000 incremental mutations.

This demonstrates the basic architecture:

```text
snapshot checkpoint
      +
incremental WAL
      ->
recovered state
```

It also demonstrates fail-closed replay for a corrupted record: the prototype does not silently accept the damaged record or subsequent bytes as valid operations.

## Important limitation

Stopping at the first corrupted middle record is safe but may discard valid operations that physically occur later in the WAL. A production design needs WAL segmentation and/or sequence-numbered frames so recovery can identify the damaged segment and make a deliberate policy decision.

## Recommended next persistence changes

- add monotonically increasing sequence numbers;
- impose maximum key/value/record lengths before allocation;
- segment the WAL;
- use CRC32C or a stronger integrity primitive as appropriate;
- store snapshot generation and last-applied WAL sequence;
- test atomic checkpoint replacement crashes;
- test disk-full and partial I/O conditions;
- add configurable durability modes;
- fuzz replay against a reference implementation.
