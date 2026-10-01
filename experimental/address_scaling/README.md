# Resolutive address scaling — isolated research

Read [AUDIT.md](AUDIT.md) first and [REPORT.md](REPORT.md) for conclusions.

No production file, root build, stable persistence, recovery, API, ABI or Memoria contract is changed. Compile this harness separately; do not merge the candidate hierarchy into the production index.

## Reproduce

From the repository root, with Linux, Python 3 and g++ 13 or equivalent:

```bash
python experimental/address_scaling/run.py --engines stable,robin,unordered,fixed1,fixed2,adaptive --out results
python experimental/address_scaling/run.py --engines compact,atomic_map,fixed3,adaptive64,adaptive1024 --workloads uniform,rho,rho_phi --out supplemental
python experimental/address_scaling/summarize.py
g++ -O3 -std=c++17 -pthread -Iexperimental/api_v86/include experimental/address_scaling/distribution.cpp experimental/api_v86/src/resolutive_index.cpp -o /tmp/bdr-distribution
/tmp/bdr-distribution > experimental/address_scaling/distribution.csv
```

The first run tests 100K, 1M and 10M with three independent query seeds (42–44), 20K hits and 20K misses checked per row. Each row is a separate process; architectures are run sequentially. There is no 100M run. Eight-bit fingerprint/poor-hash insertion is capped at 100K. Each child has a 180 s timeout, recorded explicitly in runs.jsonl.

For a small correctness check:

```bash
python experimental/address_scaling/run.py --sizes 1000 --q 200 --repeats 1 --out smoke
```

Raw data: `results/raw.csv`, `supplemental/raw.csv`; environment, commands, return codes and duration accompany each. Summaries are medians across runs, not pooled latency samples. Individual p50/p95/p99 measure hit GET; miss throughput is a separate metric.

## Compared architectures

- `stable`: unchanged legacy/fallback **ResolutiveIndex**, linked from production source. This name is retained in raw CSV; it does **not** mean the default v1 index.
- `compact`: unchanged **CompactIndex**, the default stable `Database` index. No WAL operations in this index microbenchmark.
- `atomic_map`: **std::map<string,string> state-layer control** matching the representation in DurableDatabase. It is not the full AtomicDatabase and does not include WAL, durability or its mutex.
- `robin`: growing global Robin Hood adapted from V18/V20, retaining full synthetic ID as identity.
- `unordered`: standard-library unordered_map, same encoder and mixed fingerprint hash, exact ID equality, reserve(N), normal default maximum load 1.
- `fixed1/2/3`: 64-way direct routing, respectively 1/2/3 decisions; leaves use exactly the same growing RH as the global control. Sparse node allocation; allocated routing arrays have 64 pointers.
- `adaptive`, `adaptive64`, `adaptive1024`: split a leaf at 256/64/1024 records, maximum 3 routing decisions. Redistribution is measured as part of insertion cost. These are research parameters, not proposed production defaults.

No mature C++ flat-map dependency was found in the repository; no weak substitute is presented as a mature flat-map comparator.

## Hash and identity fairness

All research structures compute the same two mixed FNV passes as the native rho/fingerprint encoder. They receive the same masked address in synthetic stress cases. Global and leaf RH use mix(fp); unordered_map uses the same mix(fp). Full generated key identity is represented by its unique uint64 ID, independent of fingerprint. Different IDs never overwrite one another. This is exact for the generator, but does not reproduce arbitrary-string storage costs. Normal keys are `K_<id>`, prefix keys add a shared long prefix. Key materialization and address computation are included in GET/PUT timing for all controls.

The native controls retain their actual full string storage, locks, byte arenas and returned string copies. Their timings are product-context measurements, **not** an equal-payload proof of hierarchy superiority. `atomic_map` retains full strings but omits the production mutex. No conventional control is deliberately assigned a worse hash than the research structures.

## Distributions

- `uniform`: sequential unique IDs, dispersed by the common encoder; not uniformly sampled random byte strings.
- `prefix`: long shared textual prefix with unique suffix.
- `zipf`: historical project's Zipf-like hot-query generator, id=floor(U^4*N). This is **not** an exact Zipf law. Population remains dispersed.
- `rho`: forcibly zero the low 6 routing bits; one primary region, independent local fingerprint unchanged.
- `rho_phi`: forcibly zero low 12 routing bits; two collapsed routing coordinates, independent local fingerprint unchanged.
- `fp8`: preserve routing entropy, truncate fingerprint to 8 bits.
- `poor`: zero all routing bits and truncate fingerprint to 8 bits.

Stress is fault injection in the common address function, not a claim to have generated a cryptographic preimage attack. Native unmodified encoders are measured only in normal/prefix/hot-query workloads.

## Measurements and limits

- PUT ops/s includes growing allocation and rehash/split; it is **not durable PUT**.
- GET throughput uses a batch loop without per-query timer; percentile loop is separate.
- `preaddress_ns` excludes key generation/encoding for matched research controls, but includes route+local leaf+memory; unavailable for native controls.
- `route_ns` times pointer navigation alone with precomputed addresses; `address_ns` includes synthetic key generation and encoding. These separately timed quantities must not be added as if they were independent components of one cold lookup.
- Depth/probes/exact comparisons/fingerprint false matches/query-weighted leaf population use a separate instrumented pass, not the timing pass. Global RH depth=0; primary-index decision is absent. Native internals are not edited for counters and return NA.
- `put_collision_events` counts occupied-slot encounters including rehashes in surviving RH tables, not unique colliding hashes. Adaptive splits discard earlier leaf counters; this is not a complete historical count. `fp_false_matches` counts observed same-fingerprint/different-ID encounters on hit queries, not all dataset fingerprint collisions. Other controls report NA/uninstrumented fields; do not interpret their zero placeholder collision events as measured absence.
- `structural_bytes_record` uses object sizes/capacities for RH/tree; allocator overhead is excluded. It is NA for native indexes and unordered_map, rather than inventing node-size estimates.
- RSS/delta/peak are measured per isolated process; peak includes rehash transients; process RSS is not exact index bytes. Baseline RSS starts after constructor allocation and before population. No allocator-independent memory attribution.
- CPU seconds are process user+system totals, including validation and measurement, not CPU utilization percentage.
- Hardware cache counters are unavailable here (perf executable absent). Cache effects are plausible causes of latency change, not measured cache-miss conclusions.
- Oracle checks 20K seeded hits and misses per configuration; it is sampled, not exhaustive verification of all 10M entries.
- Fixed architecture order, related hashed input distributions, shared host noise and warm sampled queries limit generalization. Independent seeds change queries, not the population or hash function. Prefix runs overlap briefly with harness compilation/smoke checks; see REPORT for handling.

`distribution.csv` is a full-population entropy/occupancy audit outside timing, including 10M fingerprint-pressure datasets without quadratic insertion. `duplicate_fingerprints` is N minus distinct global fingerprints, not overwrites or full-address collisions. Zipf-like queries share the uniform population. Exact collision-safe identity is still required.
