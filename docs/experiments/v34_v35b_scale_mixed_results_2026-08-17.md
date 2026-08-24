# V34 / V35b — Scale and mixed workload results

Date: 2026-08-17
Branch: `experiment/hierarchical-resolutive-addressing`
Status: experimental; `main` and tag `v0.1.0` remain unchanged.

## V34 — one million durable PUTs

Configuration:

- 1,000,000 key/value PUTs
- 8 writer threads
- client durability window 128
- BDR ticketed WAL pipeline with physical batch maximum 512
- native C/C++ APIs for market engines
- final verification of all 1,000,000 key/value pairs
- all rows completed with `errors=0`

| engine | durable PUT ops/s |
|---|---:|
| **BDR-v32/V34** | **973,535** |
| LevelDB | 855,859 |
| RocksDB | 448,009 |
| SQLite | 329,364 |
| LMDB | 220,152 |

In this runner/workload BDR was about 13.7% above LevelDB, the closest competitor.

This is a workload-specific result. It does not establish general superiority over the compared databases.

## V35b — mixed read/write, warm index

Configuration:

- 500,000 preloaded readable records
- 200,000 timed mixed operations
- 8 threads
- writes use explicit durable groups of up to 128 client operations
- BDR reads use the validated V20 `rho -> local Robin Hood` in-memory index
- BDR writes use the validated ticketed WAL pipeline
- LevelDB/RocksDB use their native engine reads and synchronous WriteBatch writes
- read profiles: uniform and hot (`80%` of reads targeting the hottest `1%` of base keys)
- all rows completed with `errors=0`

### 90% GET / 10% PUT

| engine | uniform ops/s | hot ops/s |
|---|---:|---:|
| **BDR-v35b** | **5,281,080** | **6,692,190** |
| LevelDB | 876,444 | 658,189 |
| RocksDB | 892,717 | 929,152 |

### 50% GET / 50% PUT

| engine | uniform ops/s | hot ops/s |
|---|---:|---:|
| **BDR-v35b** | **1,216,710** | **1,718,480** |
| LevelDB | 753,126 | 648,896 |
| RocksDB | 705,479 | 677,151 |

## Critical methodological limitation of V35b

V35b is an **end-to-end warm-index architecture test**, not a same-internal-layer lookup comparison.

BDR serves reads directly from its in-memory V20 index, while LevelDB and RocksDB serve reads through their complete database engines and cache layers. This is a legitimate deployment architecture comparison, but the very large read-heavy advantage must not be attributed solely to the `rho` addressing algorithm.

Also, V35b deliberately reads only pre-confirmed base keys. Newly submitted writes are validated through the WAL/recovery path, but the benchmark does not test read-your-write visibility for pending group commits.

## Conclusions retained

1. The V32/V34 concurrent durable-write advantage survives scaling from 50k to 1M records on this GitHub runner.
2. The combined V20 read index + ticketed WAL pipeline is promising for mixed workloads.
3. The read-heavy result requires stricter follow-up before any broad performance claim.
4. No result here is attributed to `phi`, `theta`, or an unproven physical interpretation.

## Next gates

- measure RSS / bytes per record for the current candidate and competitors;
- repeat mixed workload with read-your-write semantics and an explicit committed-view contract;
- add SQLite/LMDB to a later mixed comparison where API semantics can be made equivalent;
- define a new versioned WAL/snapshot format for v0.2;
- integrate checkpoint/snapshot and repeat reopen/SIGKILL validation.
