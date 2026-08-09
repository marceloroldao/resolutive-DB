# BDR Disk Format Contract — Experimental v1

Status: pre-v0.1 experimental format contract.

This document specifies the persistent structures used by `bdr/persistent_engine.py` so tests and independent implementations can reproduce recovery behavior.

## 1. WAL record

Byte order: big-endian.

Each WAL record is:

- `seq`: unsigned 64-bit sequence number
- `op`: unsigned 8-bit operation code
- `key_len`: unsigned 32-bit UTF-8 key length
- `value`: unsigned 64-bit value field
- `key`: `key_len` UTF-8 bytes
- `crc32`: unsigned 32-bit CRC of all bytes from `seq` through the end of `key`

Operation codes:

- `1`: PUT
- `2`: DELETE

Sequence numbers are globally monotonic within one database directory. Recovery rejects sequence gaps after the snapshot sequence.

A truncated final WAL record is treated as a torn tail and ignored. A complete record with an invalid CRC raises `RecoveryError`.

## 2. WAL segmentation

Segments use filenames:

`wal-000000.log`, `wal-000001.log`, ...

Segments are replayed in lexical/numeric order. Rotation occurs after the configured operation count. After a successful checkpoint, WAL history covered by that checkpoint may be retired only after the new snapshot and new active WAL have both been durably published.

## 3. Snapshot BDR2

Byte order: big-endian.

Snapshot filename: `snapshot.bin`.

Layout:

1. magic: 4 bytes, ASCII `BDR2`
2. format version: unsigned 32-bit integer
3. snapshot sequence: unsigned 64-bit integer
4. record count: unsigned 32-bit integer
5. repeated records, sorted by key:
   - key length: unsigned 32-bit integer
   - value: unsigned 64-bit integer
   - UTF-8 key bytes
6. final CRC32: unsigned 32-bit CRC over every preceding snapshot byte

Current format version: `1`.

The loader retains read compatibility with legacy `BDR1` snapshots, but new checkpoints are written as `BDR2`.

## 4. Atomic checkpoint protocol

The checkpoint protocol is:

1. fsync the active WAL;
2. write complete snapshot to `snapshot.tmp`;
3. fsync `snapshot.tmp`;
4. atomically rename `snapshot.tmp` to `snapshot.bin`;
5. fsync the database directory;
6. create/open a new post-checkpoint WAL segment;
7. fsync the directory;
8. retire old WAL segments covered by the snapshot;
9. fsync the directory again.

The purpose is to ensure that a process or host failure cannot expose a newly retired WAL history without a durable replacement snapshot.

## 5. Durability modes

`put()` and `delete()` accept `durable=True` to force an fsync before returning. Without explicit durability, `group_commit_ops` controls the automatic fsync interval. `flush(durable=True)` provides an explicit group-commit boundary.

A successful durable boundary means that all prior WAL bytes acknowledged by that boundary are expected to survive a process crash, subject to operating-system and storage-device durability guarantees.

## 6. Recovery invariants

A valid recovery must satisfy all of the following:

- snapshot checksum/version are valid;
- WAL checksums are valid except for an allowed torn final tail;
- sequence numbers after the snapshot are contiguous;
- PUT and DELETE replay deterministically;
- records at or before the snapshot sequence are not replayed twice;
- unknown operation codes are rejected.

## 7. Scope

This is an experimental format, not yet a stable public compatibility guarantee. A production format would additionally require explicit migration policy, endian/size conformance fixtures, maximum key/value limits, stronger corruption-resynchronization strategy, and compatibility tests across independent implementations.
