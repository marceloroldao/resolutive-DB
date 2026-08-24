# V32 — Concurrent durable market comparison

Date: 2026-08-16/17
Branch: `experiment/hierarchical-resolutive-addressing`
Status: experimental; `main` and `v0.1.0` unchanged.

## Contract

50,000 PUT operations, producer/client durability window 128.

BDR uses ticketed submit/wait, a dedicated WAL writer, preallocated WAL and physical batch maximum 512. Competitors use native synchronous transaction/WriteBatch boundaries of up to 128 records per client thread.

The benchmark validates all 50,000 final key/value pairs after writes. Every row below completed with `errors=0`.

## Results

| engine | 1 writer | 4 writers | 8 writers | 16 writers |
|---|---:|---:|---:|---:|
| BDR-v32 | 194,348 | **595,559** | **1,007,750** | 937,257 |
| SQLite | 296,094 | 215,009 | 68,196 | 59,964 |
| LMDB | 246,493 | 246,225 | 224,187 | 210,220 |
| LevelDB | **338,273** | 593,371 | 859,494 | **1,142,230** |
| RocksDB | 296,754 | 390,376 | 560,614 | 672,476 |

Units: durable PUT operations/second under this benchmark and runner.

## Interpretation

- Single writer: BDR is slower than all four competitors in this workload.
- Four writers: BDR and LevelDB are effectively tied; BDR is marginally ahead in this single run.
- Eight writers: BDR is first, about 17% above LevelDB and substantially above RocksDB/LMDB/SQLite.
- Sixteen writers: LevelDB becomes first; BDR is second.

This is evidence for a specific concurrency regime, not a claim that BDR is generally faster than these databases.

The result is attributed to the current engineering design (ticketed pipeline + dedicated writer + preallocation + group commit), not to an unproven physical interpretation or to `phi/theta`.

## Limitations

- One GitHub-hosted Ubuntu 24.04 environment.
- One key/value size and synthetic key distribution.
- 50k records; larger-scale runs remain required.
- Package versions are those available to the runner/Ubuntu repositories, not a claim to cover every latest upstream release.
- No mixed read/write workload in V32.
- No concurrent crash test yet for the ticketed pipeline.

## Next gate

Run real `SIGKILL` during concurrent ticketed writes (focus: 8 writers, window 128, physical batch 512), then recover from WAL only and require a strictly valid durable prefix with monotonic sequence, CRC integrity, no duplicate sequence and no acceptance of preallocated zero space.
