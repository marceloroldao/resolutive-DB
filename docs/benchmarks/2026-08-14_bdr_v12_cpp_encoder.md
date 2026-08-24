# BDR V12 — C++ Resolutive Encoder Comparison

Date: 2026-08-14  
Branch: `experiment/v0.2-cpp-encoder`  
Status: experimental; does not modify the published v0.1.0 baseline.

## Objective

Test the BDR addressing core in C++, separately from the persistence/WAL layer, and compare it against native C++ hashing under a controlled workload.

The experiment compares:

1. `std::unordered_map<std::string, uint64_t>` — native baseline.
2. partitioned fast hash — same fast deterministic hash, direct partitioning, no phase channel.
3. resolutive address — full C++ encoder computing `rho_R`, `phi`, `theta`, `f_nu`, and a 64-bit fingerprint; lookup uses `rho_R` for the partition and `(phi,fingerprint)` for local identity.

The main encoder intentionally avoids SHA-256/BLAKE2b so that cryptographic hashing cost is not confused with the cost of resolutive addressing. It uses FNV-1a followed by SplitMix-style 64-bit mixing.

Build command used locally:

```bash
g++ -O3 -std=c++17 -march=native benchmarks/bdr_v12_cpp_encoder.cpp -o bdr_v12_cpp_encoder
```

Example execution:

```bash
./bdr_v12_cpp_encoder 100000 1000000 2000000
```

## Results from the local execution

### N = 100,000

| Engine | Build (s) | Mean lookup (us) | p50 (us) | p99 (us) | Mops/s |
|---|---:|---:|---:|---:|---:|
| native `unordered_map<string>` | 0.00993 | 0.2879 | 0.250 | 0.711 | 3.109 |
| partitioned fast hash | **0.00785** | **0.2670** | **0.220** | **0.611** | **3.347** |
| resolutive `rho+phi+fp` | 0.00844 | 0.3848 | 0.341 | 0.921 | 2.400 |

### N = 1,000,000

| Engine | Build (s) | Mean lookup (us) | p50 (us) | p99 (us) | Mops/s |
|---|---:|---:|---:|---:|---:|
| native `unordered_map<string>` | 0.5409 | **0.8105** | **0.781** | **1.282** | **1.149** |
| partitioned fast hash | **0.2518** | 0.8957 | 0.851 | 1.332 | 1.078 |
| resolutive `rho+phi+fp` | 0.2562 | 1.0076 | 0.921 | 1.442 | 0.962 |

### N = 2,000,000

| Engine | Build (s) | Mean lookup (us) | p50 (us) | p99 (us) | Mops/s |
|---|---:|---:|---:|---:|---:|
| native `unordered_map<string>` | 1.0502 | **0.8584** | **0.811** | **1.362** | **1.087** |
| partitioned fast hash | **0.6047** | 1.0227 | 0.962 | 1.483 | 0.948 |
| resolutive `rho+phi+fp` | 0.6155 | 0.9954 | 0.952 | 1.473 | 0.974 |

## Interpretation

The C++ test changes the earlier interpretation from the Python-only encoder experiments, but it does not yet validate a performance advantage for the resolutive address itself.

### What improved in C++

The partitioned numeric-key structures build much faster than `unordered_map<string>` at 1M and 2M records. This is consistent with avoiding storage and hashing of full strings inside the local maps and with smaller local hash tables.

At 1M records, build time was approximately:

- native string map: 0.541 s;
- partitioned fast hash: 0.252 s;
- resolutive: 0.256 s.

So both partitioned numeric representations were about 2.1x faster to build in this run.

### What did not improve

Lookup throughput did not show a general advantage for the full resolutive encoder.

At 1M records:

- native: 1.149 Mops/s;
- resolutive: 0.962 Mops/s.

The resolutive variant was therefore about 16% slower in hit throughput in this specific run.

At 2M, the gap narrowed:

- native: 1.087 Mops/s;
- resolutive: 0.974 Mops/s.

### Effect of the phase channel

The fast-partitioned control is critical. It uses the same basic fast hash family and the same partitioned local-map topology, but does not compute/use the full phase-derived address.

At 100k it was clearly faster than the full resolutive variant. At 1M it also remained faster. At 2M, the resolutive variant slightly exceeded the fast-partitioned control in throughput, but not enough to support a robust conclusion from one execution.

Therefore the current evidence does **not** show that `phi` creates a lookup-speed advantage. In the current implementation, the additional channels primarily add encoder work.

## Scientific conclusion

This experiment supports the following constrained conclusion:

> The earlier C++ persistence advantage must not be interpreted as validation of the resolutive addressing mechanism. When the addressing core itself is implemented and tested in C++, partitioning/numeric local keys can substantially reduce build time, but the full `rho_R + phi + fingerprint` encoder does not yet outperform native `std::unordered_map<string>` for lookup hits.

The result is negative but scientifically useful. It isolates the performance benefit observed so far to partitioning, compact numeric representation, and/or persistence design rather than to the phase channel itself.

## Next required experiments

1. Repeat each configuration over multiple independent runs and report median/confidence intervals.
2. Measure encoder-only throughput to separate address computation from local lookup.
3. Test misses separately from hits.
4. Measure memory and cache misses (`perf stat`, L1/L2/L3 where available).
5. Replace the internal `unordered_map` with contiguous Robin Hood/Swiss-table-like local storage.
6. Compare address variants:
   - `rho + fingerprint`;
   - `rho + phi + fingerprint`;
   - `rho + theta/phase-derived routing + fingerprint`.
7. Test concurrency to determine whether the multidimensional address is more useful as a scheduling/sharding coordinate than as a single-thread lookup accelerator.
8. Test larger N on hardware with sufficient RAM.

## Reproducibility note

These results were obtained in the current execution environment and are not general performance claims. Hardware, compiler, standard library implementation, allocator, CPU cache hierarchy, and operating system can materially change the measurements.
