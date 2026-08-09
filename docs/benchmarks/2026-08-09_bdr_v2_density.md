# BDR V2 — Fixed-M Density Experiment

Date: 2026-08-09
Repository: `marceloroldao/resolutive-DB`

## Purpose

Test BDR V2 while holding the first-level address space fixed at `M = 100,000` buckets and increasing record count `N`, so density `lambda = N/M` rises from 0.1 to 20.

## Baseline preservation

The original user-provided script is preserved verbatim at:

`benchmarks/baselines/user_bdr_v2_as_received.py`

The original script does not complete because the population loop uses `payload = {"data": i}` while iterating as `for k in keys:`. Variable `i` is undefined, producing `NameError` before the first benchmark measurement.

The minimally corrected and instrumented reproduction is:

`benchmarks/bdr_v2_density_reproducible.py`

Correction: use `for i, key in enumerate(keys):`.

## Methodological changes for reproducibility

- deterministic query seed: `20260809 + N`;
- 2,000 hit queries per size;
- 5 timing repetitions;
- median latency reported in microseconds/query;
- GC disabled during timed sections;
- fixed `M = 100,000`;
- `phase_buckets = 65,536`;
- fidelity checked on all sampled queries;
- occupancy and maximum bucket size recorded;
- replacement count recorded to detect collisions of `(phi_quant, signature)`.

## Observed run

The following values were observed in the execution environment used during development. They are not universal performance claims and should be reproduced independently on other hardware/interpreters.

| N | lambda | BDR V2 median us | dict median us | binary median us | mean occupied-bucket size | max bucket | replacements | fidelity |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10,000 | 0.1 | 1.5960 | 0.0334 | 0.3779 | 1.0517 | 4 | 0 | 100% |
| 50,000 | 0.5 | 3.1979 | 0.0340 | 0.5149 | 1.2713 | 5 | 0 | 100% |
| 100,000 | 1.0 | 2.1328 | 0.0421 | 0.5497 | 1.5815 | 7 | 0 | 100% |
| 500,000 | 5.0 | 2.4339 | 0.0704 | 0.8231 | 5.0345 | 16 | 0 | 100% |
| 1,000,000 | 10.0 | 1.7290 | 0.0755 | 0.8238 | 10.0005 | 24 | 0 | 100% |
| 2,000,000 | 20.0 | 1.7954 | 0.0967 | 0.9649 | 20.0000 | 41 | 0 | 100% |

## Interpretation

The fixed first-level address space became substantially denser: mean occupied-bucket size increased to 20 and the largest observed bucket reached 41 entries at two million records. Despite this, sampled BDR V2 lookup latency did not rise proportionally with bucket population because there is no linear scan inside the bucket; lookup is delegated to a Python dictionary keyed by `(phi_quant, signature)`.

This is an encouraging scaling result for the two-level architecture, but it does **not** establish strict worst-case O(1). The second-level structure is a Python `dict`, whose standard performance model is expected/amortized O(1), with pathological collision/resizing cases not bounded by this benchmark. SHA-256 encoding is also a fixed per-key cost for these bounded-length synthetic keys.

## Technical findings

1. **Major improvement over V1:** linear `for` traversal within a rho bucket is removed.
2. **Deterministic addressing:** SHA-256 fixes the persistence problem created by Python's randomized string `hash()`.
3. **Channel separation:** rho, phi and signature use disjoint SHA-256 byte slices, reducing direct algebraic dependence between address dimensions.
4. **64-bit signature:** no replacement/collision was observed through two million generated keys in this run, but absence of observed collision is not proof of impossibility.
5. **Count semantics bug:** the as-received `insert()` increments `count` even if an existing `(phi_quant, signature)` slot is overwritten. The reproducible version distinguishes new insertions from replacements.
6. **Benchmark comparator:** `bisect` over a Python list is a binary-search baseline, not a real B-tree database implementation.
7. **Memory remains unmeasured:** V2 uses an outer list of 100,000 references plus up to 100,000 Python dictionaries, tuple keys and Python objects. Memory/record must be measured before any efficiency claim.

## Current engineering conclusion

BDR V2 is materially stronger than V1 as a software data structure. The observed experiment supports the statement that its lookup latency remained approximately flat over the tested density range under non-adversarial synthetic keys. It does not yet demonstrate a new asymptotic class relative to hash tables because the phase-resolution layer itself is implemented using a hash table.

## Next experiments

The next reproducible suite should test: hit vs miss latency; random variable-length keys; repeated updates; deletions/tombstones; adversarially concentrated rho values; reduced signature widths to force controlled collisions; memory per record; insertion throughput; multithreaded access; process restart/persistence; SQLite B-tree, PostgreSQL B-tree/hash indexes, Redis and DuckDB under equivalent key-value workloads. A later vector-retrieval suite should compare BDR-derived addressing separately against FAISS/HNSW using recall@k as well as latency.
