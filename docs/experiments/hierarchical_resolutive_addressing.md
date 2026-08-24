# Hierarchical Resolutive Addressing — Experimental Study

**Repository:** `marceloroldao/resolutive-DB`  
**Experimental branch:** `experiment/hierarchical-resolutive-addressing`  
**Published baseline preserved:** `v0.1.0`  
**Status:** experimental; no merge recommendation yet  
**RSMS compatibility:** terminology aligned with the Resolutive project family; computational claims are evaluated independently of physical interpretation.

## 1. Research question

This study evaluates whether a deterministic hierarchical/adaptive address trajectory

`key -> rho -> (rho,phi) -> (rho,phi,theta) -> fingerprint -> payload`

provides a measurable engineering advantage over simpler addressing and hashing strategies.

The terms `rho`, `phi`, `theta`, `f` and "antiresonance" are treated as computational hypotheses. No physical property is used as proof of database performance.

## 2. Audit of the published v0.1.0

The published Python implementation was audited directly at tag `v0.1.0`.

Observed operational path:

- `rho_R` selects the primary bucket;
- `phi` selects the secondary dictionary;
- the 128-bit fingerprint confirms exact identity;
- `theta` and `f_nu` are stored as normalized metadata and do not participate in lookup.

Therefore the operational v0.1.0 location path is approximately:

`key -> BLAKE2b -> rho_R -> phi -> fingerprint -> entity`

This motivates the experimental separation:

- Address: `A_R = (rho, phi, theta)`
- State: `S_R = (f_nu, payload, metadata, ...)`

`f_nu` is retained as state unless an experiment demonstrates that it improves location.

## 3. Experimental architecture

The C++ benchmark implements independent variants so each mode pays only for the indexes it actually uses.

### Z1

`Z1 = rho`

The primary partition is selected and the partition records are scanned for the fingerprint.

### Z2

`Z2 = (rho, phi)`

A secondary index maps `phi` to a smaller candidate list. Fingerprint remains the exact identity guard.

### Z3

`Z3 = (rho, phi, theta)`

A tertiary composite index maps `(phi,theta)` to a smaller candidate list.

### Z4 control

A fourth level `(rho,phi,theta,f)` is included only as a control. It is not assumed to be useful.

### Adaptive metadata

Per-partition structural metadata contains only routing information:

```text
preferred_depth
occupancy
max_phi_occupancy
max_phi_theta_occupancy
estimated_probe
generation
```

The payload is not duplicated in this metadata.

The current policy is deliberately simple:

- small partition -> Z1;
- larger partition whose `phi` sub-buckets are small -> Z2;
- otherwise -> Z3.

This is a first test of adaptive collision dispersion. It is not claimed to be an optimal policy.

## 4. Controls

The benchmark includes:

- `rho + fingerprint`;
- `rho + phi + fingerprint`;
- `rho + theta + fingerprint`;
- `rho + phi + theta + fingerprint`;
- fixed `Z1 -> Z2 -> Z3`;
- fixed four-level control;
- adaptive depth without structural metadata;
- adaptive partition metadata;
- simple direct-mapped cache;
- frequency counter without routing change;
- adaptive metadata + simple cache.

The simple-cache and frequency-counter controls are important because structural memory must demonstrate a different operational benefit rather than merely rename conventional caching.

## 5. Workloads

The experiment generates deterministic datasets with seed `0xB0D2A018`:

- sequential keys;
- shared-prefix strings;
- UUID-like strings;
- random strings;
- Zipf-like/hot-key query distribution;
- `hotrho`, which deterministically selects keys concentrated in a small subset of primary partitions;
- collapse control with only 64 primary buckets.

GitHub Actions executes three repetitions with `N=100000`, `Q=50000`, and `M=2048`, plus the 64-bucket collapse test.

## 6. Metrics

The CSV output records:

- build time;
- lookup mean;
- median;
- p95;
- p99;
- maximum observed latency;
- throughput;
- approximate bytes/record;
- mean and maximum occupancy;
- occupancy variance;
- distribution entropy;
- fraction of routing decisions at Z1/Z2/Z3/Z4;
- average candidate count;
- cache hit rate.

An optional `perf stat` step attempts to collect cycles, instructions, cache misses, branches and branch misses. If hosted-runner permissions reject these counters, no cache-locality claim is made from them.

## 7. Initial local ablation signal

The following values are diagnostic local runs, not the final cross-runner result. They were used to decide whether the experiment was worth moving to GitHub Actions.

For `N=100000`, `M=2048`:

### Uniform/sequential distribution

Approximate mean lookup:

- `rho+fp`: ~0.27 us;
- adaptive metadata: ~0.27 us;
- adaptive metadata + cache: ~0.25 us.

In this regime the hierarchy does not provide a large win. Extra indices can increase tail latency and memory.

### Hot primary partitions

Approximate mean lookup:

- `rho+fp`: ~0.35 us, ~1563 candidate records per lookup;
- `rho+phi+theta+fp`: ~0.24 us, ~1 candidate;
- adaptive metadata: ~0.24 us, ~1 candidate;
- adaptive metadata + cache: ~0.22 us, ~1 candidate.

This is the strongest initial signal: secondary/tertiary resolution can disperse a concentrated primary partition rather than merely move the collision.

### Zipf/hot keys

A simple cache performed better than adaptive hierarchy alone in the local diagnostic run. This is a useful negative/partitioning result:

- temporal repetition is primarily a caching problem;
- structural concentration is a partition-resolution problem.

The two mechanisms should not be conflated.

## 8. Memory result

A rejected first prototype materialized Z2, Z3 and Z4 for every partition at once. That design caused all variants to pay the memory cost of every level and made bytes/record comparisons invalid.

The implementation was replaced by per-mode/lazy index construction. This negative result supports the central hypothesis that additional dimensions should be activated only when required.

## 9. Fingerprint sweep

A separate C++ benchmark compares 64-, packed 96-, and 128-bit fingerprints.

Local diagnostic run with 200,000 fingerprints observed zero duplicates in all three variants, as expected for this scale. Representation costs were:

- 64 bit: 8 bytes;
- 96 bit packed: 12 bytes;
- 128 bit: 16 bytes.

Binary-search throughput decreased modestly as fingerprint width increased. No recommendation to reduce identity protection is made from this small run. Collision probability and scale requirements must dominate the choice.

## 10. Complexity statement

The hierarchy has a bounded number of routing decisions when configured with a maximum depth of three or four:

`routing_steps <= depth_max`.

This does **not** imply worst-case O(1) for complete lookup. Leaf candidate resolution, hash behavior, memory allocation, cache effects and adversarial distributions remain part of total complexity.

Claims are therefore separated into:

- bounded routing depth;
- expected/amortized behavior of the local indexes;
- measured empirical behavior;
- worst-case behavior, which is not proven constant.

## 11. Is this technically new?

At this stage the deterministic trajectory is operationally closest to known families of:

- multi-level/hierarchical hashing;
- adaptive partitioning;
- bucket refinement/splitting;
- per-partition routing metadata;
- local collision-dispersion strategies.

The present experiment has **not** demonstrated that the trajectory itself is a new primitive distinct from those families. The potentially differentiating engineering idea is narrower: use measured local state to activate only the resolution dimensions needed by each region while preserving a deterministic key-derived trajectory and an exact fingerprint guard.

That difference must still be benchmarked against mature adaptive hashing/partitioning implementations before any novelty claim.

## 12. Current interpretation

What currently looks useful:

1. `rho` as a cheap primary partition;
2. exact fingerprint as identity guard;
3. adaptive activation of secondary/tertiary resolution under structural concentration;
4. small per-partition metadata that remembers the appropriate depth;
5. keeping cache and structural routing as separate mechanisms.

What is not yet justified:

1. forcing `phi` into every lookup;
2. forcing `theta` into every lookup;
3. using `f_nu` as an address coordinate merely for conceptual symmetry;
4. fixed three-level routing for all partitions;
5. four levels as a default;
6. any worst-case O(1) claim;
7. any claim that Resolutive Physics proves the observed behavior.

## 13. Reproduction

Build:

```bash
g++ -O3 -std=c++20 benchmarks/bdr_v18_hierarchical_resolutive.cpp -o bdr_v18
g++ -O3 -std=c++20 benchmarks/bdr_v18_fingerprint_sweep.cpp -o bdr_v18_fp
```

Full matrix:

```bash
./bdr_v18 --n 100000 --q 50000 --buckets 2048 > results.csv
```

Collapse test:

```bash
./bdr_v18 --n 100000 --q 50000 --buckets 64 --workload sequential > collapse.csv
```

Fingerprint sweep:

```bash
./bdr_v18_fp 500000 > fingerprints.csv
```

The GitHub Actions workflow `.github/workflows/hierarchical-resolutive-addressing.yml` runs three repetitions and uploads the raw CSV plus environment metadata.

## 14. Remaining mandatory work before merge recommendation

- extract medians and confidence/dispersion from the GitHub Actions artifacts;
- compare the winning adaptive mode directly against `std::unordered_map`, the project's Robin Hood implementation and the published BDR baseline under matched hash cost;
- add phase-bit truncation and forced `rho+phi` concentration tests;
- complete cache-counter analysis with hardware performance counters when available;
- run larger scales (1M+ records);
- test concurrency and mutation-driven metadata invalidation;
- test whether structural metadata still helps when a mature flat/Robin-Hood leaf is used;
- only after in-memory acceptance, integrate with a new persistent format version and repeat WAL/recovery tests.

## 15. Provisional answer to the central question

**Does hierarchical/adaptive resolutive addressing produce a real computational advantage?**

Provisional answer: **under structural skew/hot partitions, yes, the adaptive refinement mechanism shows a measurable signal; under uniform distributions it is often neutral or worse than the simplest path.** Therefore the evidence supports *conditional activation*, not universal multi-dimensional addressing.

**Does deterministic trajectory currently constitute a technically new database primitive?**

Provisional answer: **not demonstrated.** The current mechanism is substantially related to hierarchical hashing and adaptive partition refinement. Any stronger novelty claim requires a direct comparison showing an operational property that those known techniques do not provide.

## 16. Merge decision

**DO NOT MERGE YET.**

The published `v0.1.0` remains the immutable baseline. The experimental branch should remain separate until the remaining controls, larger-scale runs and direct conventional-hash comparisons are complete.
