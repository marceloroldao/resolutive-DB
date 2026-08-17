# V29–V31 — Ticketed concurrency and durable pipeline results

Date: 2026-08-16/17
Branch: `experiment/hierarchical-resolutive-addressing`
Status: experimental; not merged into `main`; `v0.1.0` remains immutable.

## Purpose

Separate two durability contracts that earlier experiments mixed:

1. **strict per-call durability**: a call returns only after its own ticket is included in a completed `fdatasync`;
2. **ticketed/group durability**: `submit()` returns a monotonically increasing ticket and `wait(ticket)` establishes the durability boundary explicitly.

The second mode allows pipelining without pretending that an unflushed write is already durable.

## V29 — strict MPSC writer

A dedicated WAL thread receives requests from 1/2/4/8/16 producers. Each producer waits for its own ticket after every submit.

Observed result (100k operations, physical batch max 128):

- 1 writer: ~7.8k ops/s
- 2 writers: ~9.8–10.0k ops/s
- 4 writers: ~16.9–17.9k ops/s
- 8 writers: ~33.3–33.8k ops/s
- 16 writers: ~63.2–66.7k ops/s

All runs recovered exactly 100,000 records with `last_seq=100000`, `bad=0`, `missing=0`, `duplicates=0`.

Interpretation: strict synchronous durability scales with producer concurrency because concurrent producers share physical syncs, but one producer cannot form a multi-request group while it blocks on every call.

## V30 — ticketed pipeline

API model:

```text
submit(key,value) -> ticket
wait(ticket)      -> explicit durable boundary
```

Producer windows tested: 1, 8, 32, 128. Physical WAL batch max: 128.

Representative 100k results:

| writers | window | throughput ops/s | wait p99 us |
|---:|---:|---:|---:|
| 1 | 1 | 7,234 | 466 |
| 1 | 128 | 209,816 | 1,296 |
| 2 | 128 | 318,360 | 1,293 |
| 4 | 128 | 340,651 | 3,269 |
| 8 | 32 | 316,392 | 1,451 |
| 16 | 32 | 313,557 | 3,188 |

All configurations recovered 100% of records with no bad records, missing keys, or duplicates.

## V31 — physical batch sweep

Using the V30 ticketed API, physical writer batch maxima 128, 256 and 512 were tested with 50k operations.

Best observed throughput points:

| physical batch | writers | producer window | throughput ops/s | wait p99 us |
|---:|---:|---:|---:|---:|
| 128 | 4 | 128 | 337,397 | 1,932 |
| 256 | 8 | 128 | 572,430 | 4,001 |
| 256 | 16 | 32 | 502,716 | 1,368 |
| 512 | 4 | 128 | 598,925 | 3,321 |
| 512 | 8 | 128 | 853,939 | 2,077 |
| 512 | 16 | 32 | 629,502 | 1,273 |
| 512 | 16 | 128 | **973,457** | 2,294 |

Every row in the sweep recovered exactly 50,000 records with `last_seq=50000`, `bad=0`, `missing=0`, `duplicates=0`.

## Interpretation

The performance increase is not an O(1) proof and is not attributed to `phi`/`theta`. It comes from an engineering combination:

- preallocated WAL;
- one ordered WAL writer;
- explicit ticketed durability;
- producer pipelining;
- physical group commit;
- amortized `fdatasync`.

There is a clear throughput/confirmation-latency tradeoff. A larger producer window and physical batch increase throughput but move the explicit durability boundary farther from each individual submission.

## Current candidate API direction

The v0.2 experiment should expose durability semantics explicitly rather than hiding them:

```text
put_sync(k,v)          # strict durable return
submit(k,v) -> ticket  # pipelined
wait(ticket)           # explicit durable boundary
put_batch(...)         # explicit transactional/group boundary when added
```

## Required next gate

Run an equivalent concurrent durable benchmark against SQLite, LMDB, LevelDB and RocksDB. Match the durability boundary (batch/window) and use native APIs. Do not compare strict per-call BDR durability with a competitor's batched transaction or vice versa.
