# BDR V2 vs Flat Hash with the Same SHA-256 Cost

Date: 2026-08-09

## Objective

Measure the storage-layout effect independently from the encoder cost.

Both competitors compute exactly one SHA-256 per operation:

- **BDR V2**: SHA-256 -> rho bucket -> Python dict keyed by `(phi, signature)`.
- **Flat hash control**: SHA-256 -> first 192 digest bits -> one Python dict.

The test therefore does **not** compare SHA-256 against Python's built-in string hashing. It compares a two-level BDR layout against a single-level hash layout under approximately equal digest cost.

## Reproduction

```bash
python benchmarks/bdr_v2_vs_flat_hash_same_sha.py
```

Default parameters:

- `M = 100000` fixed rho buckets
- `phase_buckets = 65536`
- `N = 10,000 ... 2,000,000`
- hit and miss lookups
- repeated timing; median latency reported
- deterministic PRNG seed

CSV output:

```text
benchmarks/results/bdr_v2_vs_flat_hash_same_sha.csv
```

## Observed run

The following values were observed in the development environment used for this experiment. Absolute timings are machine-dependent; the script is the source of truth for reproduction.

| N | lambda | BDR hit us | Flat hit us | BDR miss us | Flat miss us | BDR insert s | Flat insert s | Fidelity |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10,000 | 0.1 | 2.156 | 0.857 | 1.446 | 0.750 | 0.031 | 0.011 | 100% |
| 50,000 | 0.5 | 2.270 | 1.201 | 1.489 | 0.844 | 0.242 | 0.106 | 100% |
| 100,000 | 1 | 2.367 | 1.177 | 1.623 | 0.766 | 0.363 | 0.168 | 100% |
| 500,000 | 5 | 2.332 | 1.272 | 1.501 | 0.749 | 2.221 | 0.865 | 100% |
| 1,000,000 | 10 | 2.436 | 1.204 | 1.572 | 0.796 | 4.671 | 1.795 | 100% |
| 2,000,000 | 20 | 2.041 | 1.094 | 1.685 | 0.749 | 12.078 | 4.561 | 100% |

At `N=2,000,000`, average occupancy of occupied rho buckets was exactly 20 and the largest observed rho bucket contained 44 entries.

## Interpretation

### 1. Scaling behavior

BDR hit latency remained in roughly the same microsecond band over a 200x increase in N. This is consistent with expected/amortized O(1) behavior under the current implementation.

The flat hash control also remained effectively flat, as expected.

### 2. Absolute speed

The flat control was faster for both hit and miss lookups at every tested N. It was also materially faster for insertion.

This means the current BDR V2 organization does **not** yet demonstrate a raw performance advantage over a conventional flat hash table when both pay the same SHA-256 cost.

### 3. Why BDR V2 is slower

BDR V2 performs additional work after hashing:

1. integer extraction and modulo for rho;
2. integer extraction and modulo for phi;
3. first-level Python list access;
4. second-level Python dictionary lookup using a tuple key;
5. allocation/retention of many small Python dictionaries.

The flat control performs one dictionary lookup with one fingerprint key.

### 4. What remains potentially useful

The BDR decomposition may still provide engineering value if rho partitioning enables capabilities not measured here, for example:

- sharding and locality;
- independent bucket locking;
- NUMA-aware placement;
- storage-tier assignment;
- bounded local rebuilds;
- parallelization by rho region;
- phase-specific compression or persistence.

These are hypotheses for future experiments, not established advantages.

## Complexity statement

The current implementation uses Python dictionaries at the second level. Therefore the defensible statement remains:

> expected/amortized O(1) lookup under normal hash-table assumptions.

It is not a proof of worst-case strict O(1).

## Next experiment

The next version should remove the internal Python `dict` and compare at least three local-resolution strategies inside each rho bucket:

1. open addressing;
2. Robin Hood hashing;
3. compact sorted phase array with binary search.

The key question is whether rho partitioning can improve cache locality, tail latency, memory efficiency, concurrency, or rebuild cost enough to justify the extra address decomposition.
