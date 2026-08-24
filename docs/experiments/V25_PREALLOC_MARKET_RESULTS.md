# V25 — Preallocated WAL market benchmark

Status: **experimental**. This result does not replace `v0.1.0` and is not yet a release candidate.

## Scope

V25 combines the V20/V22 in-memory resolutive index path (`key -> rho partition -> local Robin Hood -> fingerprint -> payload`) with a preallocated WAL written by offset (`pwrite`) and `fdatasync` at the requested batch boundary.

The benchmark compares BDR-v25 with SQLite, LMDB, LevelDB, and RocksDB in the same C++ executable. All engines returned `errors=0` in the recorded runs.

## Median of 3 runs, N=10,000

| Engine | Batch | PUT ops/s | GET ops/s |
|---|---:|---:|---:|
| BDR-v25 | 1 | 16,238.7 | 4,624,510 |
| SQLite | 1 | 10,535.1 | 245,678 |
| LMDB | 1 | 5,839.05 | 3,938,180 |
| LevelDB | 1 | 4,859.59 | 2,326,430 |
| RocksDB | 1 | 4,779.3 | 643,033 |
| BDR-v25 | 32 | 263,892 | 5,554,270 |
| SQLite | 32 | 105,826 | 243,401 |
| LMDB | 32 | 130,895 | 4,011,960 |
| LevelDB | 32 | 127,818 | 2,358,600 |
| RocksDB | 32 | 137,040 | 624,575 |
| BDR-v25 | 128 | 454,060 | 5,713,630 |
| SQLite | 128 | 281,753 | 245,083 |
| LMDB | 128 | 345,738 | 4,079,380 |
| LevelDB | 128 | 440,163 | 2,342,680 |
| RocksDB | 128 | 412,201 | 620,882 |

At N=10,000 median, BDR-v25 leads PUT throughput at batch 1, 32, and 128, and leads GET throughput in this workload. The batch-128 PUT margin over LevelDB is small (~3.2%), so it must not be treated as a universal performance claim.

## Larger run, N=50,000

| Engine | Batch | PUT ops/s | GET ops/s |
|---|---:|---:|---:|
| BDR-v25 | 1 | 15,672.8 | 4,323,310 |
| SQLite | 1 | 10,964 | 242,415 |
| LMDB | 1 | 5,328.1 | 3,521,360 |
| LevelDB | 1 | 4,849.87 | 2,136,570 |
| RocksDB | 1 | 4,706.88 | 511,814 |
| BDR-v25 | 32 | 256,308 | 4,676,910 |
| SQLite | 32 | 178,625 | 240,149 |
| LMDB | 32 | 137,993 | 3,536,700 |
| LevelDB | 32 | 137,109 | 2,160,270 |
| RocksDB | 32 | 133,982 | 531,912 |
| BDR-v25 | 128 | 475,290 | 4,431,360 |
| SQLite | 128 | 365,551 | 240,364 |
| LMDB | 128 | 364,004 | 3,601,510 |
| LevelDB | 128 | 440,619 | 2,143,170 |
| RocksDB | 128 | 397,007 | 524,169 |

At N=50,000, BDR-v25 again leads PUT and GET throughput for all three tested batch sizes in this specific runner/workload.

## Interpretation limits

These numbers are evidence for this implementation and benchmark only. They do **not** establish that Resolutive Physics or any physical analogy proves database performance. V25 also benefits from workload knowledge in its experimental constructor: partition count, partition capacities, value array size, and WAL reservation are sized from `N`; this must be removed or replaced by an online growth policy before production claims.

The current GET path is memory-resident after insertion; the benchmark is not a cold-start/on-disk lookup comparison. The current WAL records CRC and sequence information but V25 still requires independent replay/recovery validation, torn-write/truncation tests, crash injection, restart reconstruction, update/delete semantics, concurrent readers/writers, and larger/adversarial key distributions.

## Promotion gate

Do not promote V25 directly to v0.2. The next experimental gate is:

1. implement WAL replay that reconstructs the index and payloads without knowing `N` in advance;
2. validate clean restart and crash/truncated-tail recovery;
3. verify CRC/sequence rejection of corrupted records;
4. rerun the market benchmark after recovery/restart so GET is measured on reconstructed state;
5. test online partition/WAL growth and concurrency;
6. preserve negative results and regressions.
