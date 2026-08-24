# BDR v17 — Full Resolutive Address Ablation

Date: 2026-08-14
Branch: `experiment/v0.2-cpp-encoder`

## Goal

Test the computational contribution of the complete resolutive-inspired address and separate useful components from neutral or harmful ones.

The tested address is represented as:

`A_R = (rho, phi, theta, f, coherence, fingerprint)`

The benchmark distinguishes two conceptually different classes:

1. **Address dimensions** — `phi`, `theta`, `f`, coherence tag.
2. **Regional dynamics** — measured partition density controls the local representation (`compact sorted vector` vs `unordered_map`).

This is a computational ablation. It does **not** constitute validation of the underlying physical interpretation.

## Static address ablations

Variants:

- `rho + fingerprint`
- `rho + phi + fingerprint`
- `rho + theta + fingerprint`
- `rho + f + fingerprint`
- `rho + phi + theta + f + fingerprint`
- full address plus an independent coherence tag

All variants use the same deterministic fast-hash family, the same 65,536 coarse `rho` regions, the same key set and the same local hash-map representation. Therefore differences mainly measure address composition overhead/distribution effects.

## Density-adaptive representation

For the adaptive experiment, actual occupancy of each `rho` region is measured after encoding. Regions at or below a threshold use a compact sorted vector; denser regions use a hash map.

This operationalizes the resolutive-inspired hypothesis that **local density should influence structure and dynamics**, rather than treating `rho` only as a renamed hash bucket.

## Repeated 1M-record results

Three complete runs were executed with 1,000,000 keys and 200,000 random successful lookups per variant.

### Static variants — lookup latency

Representative medians across the three runs:

- `rho + fp`: ~0.824 us/op
- `rho + phi + fp`: ~0.780 us/op
- `rho + theta + fp`: ~0.760 us/op
- `rho + f + fp`: ~0.758 us/op
- `rho + phi + theta + f + fp`: ~0.774 us/op
- `full + coherence`: ~0.794 us/op

Interpretation: extra address channels sometimes improve distribution enough to offset their arithmetic cost, but no single `phi`, `theta`, `f`, or coherence component dominates consistently. At this stage they should be treated as experimental channels, not individually validated advantages.

### Density-adaptive full address

Representative medians:

- threshold 8: ~0.774 us/op
- threshold 16: ~0.593 us/op
- threshold 24: ~0.467 us/op
- threshold 32: ~0.455 us/op

Compared with the median `rho + fp` baseline (~0.824 us/op), threshold 32 improves lookup latency by roughly **45%**, while threshold 24 improves it by roughly **43%**.

P99 also improved. Typical p99 was around 1.18–1.29 us for the static baseline family and around 0.80–0.85 us for adaptive thresholds 24–32.

At 1M records, threshold 32 classified 65,533 of 65,536 regions as compact and 3 as hash regions, while threshold 24 produced 64,662 compact and 874 hash regions. This indicates that the tested average occupancy (~15.3 records/region) strongly favors compact local layouts.

## 2M directional result

At 2M records (~30.5 records/region average occupancy), the best threshold shifted upward. A threshold of 32 remained useful in the completed runs and achieved ~0.64–0.79 us/op versus ~0.82–0.85 us/op for `rho + fp`, while lower thresholds increasingly selected hash maps and lost their advantage.

This is consistent with the hypothesis that the crossover should depend on actual local density. More repetitions are required before publishing a 2M median.

## What currently looks useful

### Strong signal

**Measured density controlling the local data structure.**

This produced the largest and most repeatable gain in the current ablation. It is therefore a candidate to remain in the BDR architecture.

### Weak / conditional signal

**`phi`, `theta`, and `f` as extra local-address channels.**

They sometimes improve static lookup by a few percent, but results are not monotonic and can be explained by generic hash decorrelation. They remain experimental.

### No demonstrated benefit yet

**Independent coherence tag in the normal successful-lookup path.**

With a 64-bit fingerprint already present, adding the coherence tag did not provide a repeatable latency advantage. It may still be useful for integrity/collision diagnostics, but should not be retained in the hot path solely on performance grounds.

### Potentially harmful if used blindly

**Always using the full address.**

At larger densities the full `rho + phi + theta + f + fp` key can be slower than simpler variants. More dimensions are not automatically better.

## Architectural conclusion

The strongest computational interpretation of the resolutive concept so far is not “every record must always carry every coordinate”. It is:

`coarse address -> measure local state/density -> choose the local representation appropriate to that state`

In other words, the useful part presently looks like **state-dependent local structure**, while the individual semantic labels `phi`, `theta`, and `f` still need independent controls to distinguish a resolutive-specific effect from generic entropy/distribution effects.

## Next experiments

1. Neutral controls: replace `phi/theta/f` with anonymous decorrelated channels `h2/h3/h4` and compare bit-for-bit entropy budgets.
2. Mixed read/write workload to test dynamic migration between compact and hash representations.
3. Introduce a maintenance interval `tau_i` derived from measured mutation rate and density, then ablate adaptive time vs fixed maintenance cadence.
4. Concurrent workload: use `f`/phase lanes as potential contention domains and compare against ordinary sharding.
5. Real GET benchmark against SQLite, LMDB, LevelDB and RocksDB using the selected adaptive BDR structure.
6. Measure RSS/cache misses, not only latency, because compact local representation may derive much of its benefit from memory locality.

## Reproduction

```bash
g++ -O3 -std=c++20 benchmarks/bdr_v17_full_resolutive_ablation.cpp -o bdr_v17
./bdr_v17 1000000
./bdr_v17 2000000
```

Use multiple repetitions and report medians. CPU, compiler, allocator and cache state can materially affect microbenchmarks.
