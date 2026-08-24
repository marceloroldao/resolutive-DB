# BDR v0.2 experimental — V26 recovery and V27 SIGKILL results

Date: 2026-08-15

Branch: `experiment/hierarchical-resolutive-addressing`

Published baseline `v0.1.0` remains immutable.

## V26 — autonomous WAL recovery

Objective: reconstruct the durable key/value state from the WAL only, without reusing prior in-memory index state.

The WAL uses monotonic sequence numbers, CRC32 over key/value, preallocated storage, and conservative prefix recovery.

### Results

| Workload | Case | Good records | Bad | Last seq | Truncated tail | Payload errors |
|---|---|---:|---:|---:|---:|---:|
| 10k, batch 128 | clean | 10,000 | 0 | 10,000 | no | 0 |
| 10k, batch 128 | truncated tail | 9,999 | 0 | 9,999 | yes | 0 |
| 10k, batch 128 | mid-file corruption | 5,000 | 1 | 5,000 | no | 0 before corruption |
| 100k, batch 128 | clean | 100,000 | 0 | 100,000 | no | 0 |
| 100k, batch 128 | truncated tail | 99,999 | 0 | 99,999 | yes | 0 |
| 100k, batch 128 | mid-file corruption | 50,000 | 1 | 50,000 | no | 0 before corruption |
| 100k, batch 1 | clean | 100,000 | 0 | 100,000 | no | 0 |
| 100k, batch 1 | truncated tail | 99,999 | 0 | 99,999 | yes | 0 |
| 100k, batch 1 | mid-file corruption | 50,000 | 1 | 50,000 | no | 0 before corruption |

Interpretation: clean recovery reconstructed all records exactly. Tail truncation preserved the complete prefix and was explicitly classified as truncation. Mid-file corruption stopped recovery conservatively at the first invalid record; later bytes were not trusted.

## V27 — real POSIX SIGKILL

Objective: terminate a writer with `SIGKILL` while it is writing/syncing a preallocated WAL, then reopen the file and accept only a valid durable prefix.

The child writer used batch size 128 and up to 1,000,000 records. Parent process issued `SIGKILL` at multiple timings.

| Kill delay | Signal | Good records recovered | Last seq | Bad | Partial | Valid prefix |
|---:|---:|---:|---:|---:|---:|---:|
| 2 ms | 9 | 384 | 384 | 0 | no | yes |
| 5 ms | 9 | 1,664 | 1,664 | 0 | no | yes |
| 10 ms | 9 | 3,840 | 3,840 | 0 | no | yes |
| 20 ms | 9 | 6,528 | 6,528 | 0 | no | yes |

All recovered records passed sequence, CRC and exact key/value validation. The preallocated zero-filled region was treated as end-of-log, not as data.

## Current assessment

These experiments strengthen the V25/V26 persistent-engine candidate: preallocation improves write throughput while conservative replay can still identify an exact valid prefix after truncation, corruption and abrupt process death.

They do **not** establish power-loss durability across all filesystems/storage devices, nor production readiness. `fdatasync()` semantics, filesystem behavior and hardware caches remain part of the durability contract and must be documented.

## Next acceptance gate

1. multiwriter/concurrent PUT workload with a single durable WAL ordering;
2. validate monotonic sequence assignment and absence of lost/duplicate records;
3. compare 1/2/4/8 writer throughput and p50/p95/p99 latency;
4. repeat crash recovery while multiple writers are active;
5. then run larger market comparisons and checkpoint/snapshot integration.

No merge to `main` is recommended yet.
