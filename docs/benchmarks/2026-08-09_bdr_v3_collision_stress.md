# BDR V3 — Collision-width and density stress

Date: 2026-08-09
Repository: `marceloroldao/resolutive-DB`

## Goal

Test two failure modes independently:

1. increasing rho density `lambda=N/M` while preserving a wide `(phi, signature)` address;
2. reducing phase/signature address width to force collisions at fixed `N/M`.

This is an adversarial benchmark. It is designed to locate degradation thresholds, not to assume constant-time behavior.

## Method

- deterministic SHA-256 encoder;
- `rho`: SHA-256 bytes 0..7 modulo M;
- `phi`: bytes 8..15 modulo configured `phase_buckets`;
- `signature`: bytes 16..23 masked to configured bit width;
- second-level structure: Python `dict[(phi, signature)]`;
- queries sampled deterministically;
- median of 5 repetitions;
- `perf_counter_ns` timing;
- GC disabled during measured lookup loops;
- fidelity checked by exact key recovery;
- overwrites count distinct keys mapped to the same full `(rho, phi, signature)` slot.

Because the second level is a Python dictionary, these measurements support expected/amortized O(1) lookup semantics, not a proof of strict worst-case O(1).

## Address-width stress

Fixed: `N=500,000`, `M=100,000`, `lambda=5`.

| phase buckets | signature bits | lookup (us) | fidelity | overwrites | stored | mean occupied rho bucket | max rho bucket |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 65,536 | 64 | 2.139 | 100.00% | 0 | 500,000 | 5.034 | 17 |
| 1,024 | 32 | 2.238 | 100.00% | 0 | 500,000 | 5.034 | 17 |
| 256 | 24 | 1.708 | 100.00% | 0 | 500,000 | 5.034 | 17 |
| 64 | 16 | 2.232 | 100.00% | 0 | 500,000 | 5.034 | 17 |
| 16 | 12 | 2.150 | 100.00% sampled | 20 | 499,980 | 5.034 | 17 |
| 4 | 8 | 2.134 | 99.74% sampled | 1,228 | 498,772 | 5.022 | 17 |

### Interpretation

Latency did not reveal the failure first. Integrity did.

The first observed full-address collisions appeared at `phase_buckets=16` with a 12-bit signature. At `phase_buckets=4`, `signature_bits=8`, 1,228 distinct-key overwrites occurred among 500,000 attempted inserts and sampled query fidelity dropped to 99.74%.

This demonstrates that a flat lookup curve is insufficient as a correctness criterion. Address entropy must be tracked together with latency.

## Density stress

Fixed: `N=500,000`, `phase_buckets=65,536`, `signature_bits=64`.

| M | lambda=N/M | lookup (us) | fidelity | overwrites | mean occupied rho bucket | max rho bucket |
|---:|---:|---:|---:|---:|---:|---:|
| 100,000 | 5 | 1.948 | 100% | 0 | 5.034 | 17 |
| 20,000 | 25 | 1.691 | 100% | 0 | 25.000 | 47 |
| 10,000 | 50 | 1.802 | 100% | 0 | 50.000 | 79 |
| 5,000 | 100 | 1.772 | 100% | 0 | 100.000 | 136 |

### Interpretation

With a 64-bit signature and 65,536 phase buckets, increasing rho density from `lambda=5` to `lambda=100` did not produce measurable monotonic lookup degradation in this run, even though the largest rho bucket reached 136 records.

The reason is architectural: rho only selects the first-level bucket; the second-level Python dictionary performs direct expected/amortized lookup by `(phi, signature)`. Therefore rho-bucket population alone does not imply a linear scan cost.

## Main conclusion

The V2/V3 structure is robust under high rho density when the secondary address retains sufficient entropy. Its primary observed failure mode is not density itself but exhaustion of the combined `(phi, signature)` address space, which causes exact-slot collisions and overwrites.

The result is useful, but it does **not** establish a new worst-case complexity class relative to conventional hash indexing because the secondary resolution mechanism is still a hash table.

## Next experiment

The next engineering step should test a V3/V4 design in which the secondary lookup is not a Python dictionary. Candidate experiments:

1. compact open-addressed tables local to each rho bucket;
2. Robin Hood hashing local to each rho bucket;
3. cuckoo-style two-choice phase addressing;
4. direct segmented arrays for quantized phi with sparse allocation;
5. a control implementation using a single conventional hash table with the same SHA-256 encoding cost.

The control in item 5 is essential: it will tell us whether the two-level resolutive layout provides a measurable advantage over an equivalently encoded ordinary hash table.
