# BDR experimental C++ API — V56+

> Experimental only. This directory is not a release and does not change the published v0.1.0 baseline.

## Purpose

This prototype joins the API contract, the current in-memory resolutive index candidate, and the BDR3/BDW3 persistence path in one engine.

Operational lookup path:

```text
key
  -> deterministic encoder
  -> rho partition
  -> local Robin Hood table
  -> 64-bit fingerprint + full-key confirmation
  -> value
```

`rho` is an experimental computational partition coordinate. The implementation does not claim that Resolutive Physics proves database performance. `phi`, `theta`, and `f_nu` are intentionally absent from the hot lookup path because prior ablation work did not justify keeping extra address dimensions there.

## Durability contract

```cpp
auto db = bdr::Database::open("database-dir");

auto ticket = db->put("key", "value"); // visible in-memory
// ...
db->wait(ticket);                       // durable through this sequence

db->put_sync("key2", "value2");      // convenience: put + wait
```

The API deliberately separates visibility from durability:

- `put()` / `erase()` mutate the in-memory index and return a monotonic `Ticket`.
- `wait(ticket)` returns only after every sequence up to the ticket has passed the BDW3 group-commit durability frontier.
- `put_sync()` / `erase_sync()` combine mutation and durability confirmation.
- `sync()` waits for the current sequence frontier.
- `checkpoint()` writes an atomic BDR3 snapshot, rotates to a fresh BDW3 WAL, then retires covered WAL segments.

## Persistence

Current experimental persistence stack:

```text
API
 -> ticketed writer queue
 -> group commit
 -> BDW3 framed WAL
    - total_len
    - sequence
    - operation
    - key/value lengths
    - frame-header CRC
    - payload
    - full-record CRC
 -> FALLOC_FL_KEEP_SIZE preallocation when enabled
 -> BDR3 deterministic snapshot
 -> fsync + atomic rename + directory fsync
```

A torn final WAL frame is recoverable only as an unconfirmed tail. Corruption inside a complete frame is an error, not a safe-prefix condition.

## Index design

The V56 index uses dynamic Robin Hood tables per `rho` partition. Each local table supports:

- dynamic resize;
- full-key confirmation after fingerprint match;
- updates without increasing cardinality;
- backward-shift deletion;
- partition-local shared/exclusive locking;
- deterministic snapshot enumeration;
- runtime statistics (`IndexStats`).

This is intentionally `rho + fingerprint`, not the older `rho + phi + theta + ...` experimental address. Extra dimensions may return only if future benchmarks show an objective advantage.

## Reproducible gates

GitHub Actions workflows currently cover:

- `v56-api-resolutive-index.yml`: API semantics, resize, update, delete, checkpoint/reopen and torn-tail repair.
- `v57-api-multiwriter.yml`: 1/2/4/8/16 writers and durability windows 1/32/128 through the API.
- `v58-api-sigkill.yml`: real SIGKILL during 8-writer operation followed by reopen and continued writes.
- `v59-api-market.yml`: API-level comparison against SQLite, LMDB, LevelDB and RocksDB using synchronous/durable batch contracts.

Results must be taken from generated workflow artifacts. Do not copy benchmark claims into release documentation until the corresponding artifacts are verified and preserved.

## Publication gate

No new BDR release is planned from this prototype alone. The agreed project gate is: **no publication before a usable, documented, tested API exists**. Even after the API exists, release candidacy requires the API-level crash, concurrency, recovery and market-comparison gates to pass reproducibly.
