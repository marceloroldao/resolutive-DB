# BDR v0.2 — Engine Selection Notes (2026-08-14)

Branch: `experiment/hierarchical-resolutive-addressing`

Published baseline `v0.1.0` remains immutable. No merge recommendation yet.

## Objective

Select only the parts of the hierarchical/resolutive addressing experiments that improve engineering metrics. Physical terminology is not used as proof of performance.

## V19 result — reject unconditional phi split

V19 combined:

- `rho` primary partition;
- local Robin Hood for normal partitions;
- `phi` split for dense partitions.

GitHub Actions results showed that the `phi` split is harmful when the local Robin Hood table is already healthy.

Representative 100k `hotrho` result:

- global Robin Hood: ~0.118 us mean lookup;
- rho-partitioned Robin Hood: ~0.124 us;
- adaptive Compact/Robin/phi split: ~0.272 us.

Approximate index memory for the phi-split mode reached ~224 B/record in that workload.

Decision: **do not place `phi` in the default hot path.** Keep it only as an experimental fallback for a future failure mode that a strong local hash table cannot handle.

## V20 result — rho density target

V20 removed the unconditional hierarchical split and tested a simpler engine:

`key -> rho -> local Compact/Robin-Hood -> fingerprint`

The number of rho partitions is derived from a target mean density. Tested target densities:

- 16;
- 32;
- 64;
- 128;
- 256 records/partition.

### 100k records

For sequential keys, target densities 32-256 clustered around ~0.139-0.140 us mean lookup. The matched global Robin Hood baseline was ~0.137 us.

Target density 16 reduced memory substantially (~21.7 B/record) but lookup slowed to ~0.187 us. This is useful as a potential memory-oriented profile, not the default speed profile.

### 1M records

Representative sequential results:

- target 16: ~0.477 us, ~28.6 B/record;
- target 32: ~0.339 us, ~47.4 B/record;
- target 64: ~0.323 us, ~48.9 B/record;
- target 128: ~0.314 us, ~50.0 B/record;
- target 256: ~0.294 us, ~50.37 B/record;
- global Robin Hood: ~0.291 us, ~50.33 B/record.

Thus target density ~256 nearly matches the global Robin Hood baseline in single-thread lookup and memory at 1M records while retaining rho partition boundaries for future concurrency/local-state experiments.

### 1M hot-rho workload

Target density ~256 measured ~0.279 us mean lookup in the recorded run. This is the most promising current speed profile, but it must be compared in the same executable/run against the global and modern sharded controls before a superiority claim.

## Current selected architecture

The current v0.2 candidate for further testing is:

`key -> deterministic digest -> rho -> local Robin Hood -> fingerprint -> payload`

with:

- target rho density around 256 for the speed profile;
- a Compact/Robin-Hood memory profile retained as an optional tradeoff;
- no mandatory `phi`, `theta`, or `f_nu` in the lookup path;
- `phi`/`theta` kept only as experimental emergency refinement dimensions;
- structural metadata retained only if it controls a measured routing/structure decision.

## What is rejected for now

- mandatory Z1 -> Z2 -> Z3 traversal;
- unconditional phi split;
- theta in every lookup;
- f_nu as an address coordinate without measured location benefit;
- four-level routing as a default;
- worst-case O(1) claims.

## Next acceptance gate

Before any merge toward v0.2:

1. integrate the target-density-256 rho/local-Robin-Hood index with the durable WAL path;
2. use a persistent file handle and real `fdatasync`/equivalent durability barrier;
3. benchmark batch sizes 1/32/128;
4. compare end-to-end PUT and real keyed GET against SQLite, LMDB, LevelDB and RocksDB under documented durability semantics;
5. repeat at multiple N and record medians across independent runs;
6. compare against the published v0.1.0 baseline and a same-hash conventional control;
7. preserve all negative results.

The previous V16 market benchmark remains useful for WAL methodology, but its BDR GET result is not a valid keyed lookup comparison because it read from a materialized vector. Any new end-to-end benchmark must correct this.

## Current decision

**Do not merge yet.**

V20 is a better candidate than V19 for the v0.2 core, but the persistent end-to-end market comparison is still mandatory before promotion.
