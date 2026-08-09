# BDR V3 — Local Engine Benchmark

Date: 2026-08-09

## Goal

Evaluate whether removing the Python `dict` from the second level of the BDR improves latency or tail behavior while preserving deterministic resolutive addressing.

All engines use the same deterministic SHA-256 encoder. The benchmark therefore focuses on the local storage engine rather than encoder cost.

Compared engines:

1. `bdr_v2_dict`: direct `rho` bucket + Python `dict[(phi, signature)]`.
2. `bdr_v3_linear`: direct `rho` bucket + local linear-probing open addressing.
3. `bdr_v3_robin`: direct `rho` bucket + local Robin Hood hashing.
4. `flat_hash`: one flat Python dictionary keyed by a SHA-256-derived fingerprint.

Fixed first-level bucket count: `M = 10,000`.

Metrics: insertion time, hit/miss median, p95 and p99 latency, and lookup fidelity.

## Observed results in the reference run

The following values were measured in the development runtime and are not universal performance claims. Re-run the committed script on target hardware before comparison.

### N = 10,000 (lambda = 1)

| Engine | Insert s | Hit median us | Hit p95 | Hit p99 | Miss median us | Fidelity |
|---|---:|---:|---:|---:|---:|---:|
| BDR V2 dict | 0.020 | 1.797 | 2.194 | 2.803 | 1.476 | 100% |
| V3 linear | 0.039 | 2.084 | 2.610 | 3.118 | 1.629 | 100% |
| V3 Robin Hood | 0.046 | 2.062 | 2.605 | 3.307 | 1.610 | 100% |
| Flat hash | 0.102* | 1.749 | 2.520 | 8.934* | 1.530 | 100% |

`*` Small-N runs are especially sensitive to interpreter/runtime noise and should not be used alone for conclusions.

### N = 100,000 (lambda = 10)

| Engine | Insert s | Hit median us | Hit p95 | Hit p99 | Miss median us | Miss p99 | Fidelity |
|---|---:|---:|---:|---:|---:|---:|---:|
| BDR V2 dict | 0.306 | 2.285 | 3.622 | 3.996 | 1.705 | 2.425 | 100% |
| V3 linear | 0.578 | 2.496 | 3.437 | 4.433 | 2.020 | 4.453 | 100% |
| V3 Robin Hood | 0.529 | 2.680 | 3.786 | 4.955 | 2.017 | 3.495 | 100% |
| Flat hash | 0.388 | 1.987 | 2.630 | 3.007 | 1.585 | 2.105 | 100% |

### N = 300,000 (lambda = 30)

| Engine | Insert s | Hit median us | Hit p95 | Hit p99 | Miss median us | Miss p99 | Fidelity |
|---|---:|---:|---:|---:|---:|---:|---:|
| BDR V2 dict | 0.623 | 2.275 | 2.803 | 3.365 | 1.705 | 2.335 | 100% |
| V3 linear | 1.544 | 2.575 | 3.536 | 4.594 | 2.071 | 5.489 | 100% |
| V3 Robin Hood | 1.824 | 2.835 | 3.732 | 4.290 | 2.109 | 3.624 | 100% |
| Flat hash | 0.710 | 2.535 | 3.337 | 3.765 | 1.636 | 2.192 | 100% |

### N = 500,000 (lambda = 50)

| Engine | Insert s | Hit median us | Hit p95 | Hit p99 | Miss median us | Miss p99 | Fidelity |
|---|---:|---:|---:|---:|---:|---:|---:|
| BDR V2 dict | 1.577 | 3.519 | 4.265 | 4.838 | 1.799 | 2.569 | 100% |
| V3 linear | 2.917 | 2.761 | 3.620 | 4.655 | 2.074 | 5.820 | 100% |
| V3 Robin Hood | 3.124 | 2.880 | 3.690 | 4.494 | 2.099 | 3.754 | 100% |
| Flat hash | 1.219 | 2.948 | 3.584 | 4.193 | 1.638 | 2.273 | 100% |

### N = 1,000,000 (lambda = 100)

| Engine | Insert s | Hit median us | Hit p95 | Hit p99 | Miss median us | Miss p99 | Fidelity |
|---|---:|---:|---:|---:|---:|---:|---:|
| BDR V2 dict | 2.217 | 2.552 | 3.289 | 4.006 | 1.869 | 2.783 | 100% |
| V3 linear | 5.442 | 2.843 | 3.711 | 4.992 | 2.101 | 6.804 | 100% |
| V3 Robin Hood | 6.440 | 3.029 | 3.941 | 4.719 | 2.182 | 4.400 | 100% |
| Flat hash | 2.675 | 2.998 | 3.669 | 4.436 | 1.650 | 2.378 | 100% |

## Interpretation

### 1. Removing the nested dict does not automatically make BDR faster in Python

The local open-addressing implementations are written in Python, whereas Python's built-in `dict` is highly optimized C code. The V3 engines therefore pay Python-loop overhead for each probe.

This means the current result is an implementation result, not evidence that open addressing is intrinsically inferior.

### 2. Robin Hood improves miss-tail behavior relative to simple linear probing

At high density, linear probing showed a noticeably worse miss p99. For `N = 1,000,000`, the observed miss p99 was approximately:

- Linear probing: 6.804 us
- Robin Hood: 4.400 us

This is consistent with Robin Hood hashing reducing variance in probe distances. The absolute latency remained slower than the V2/flat controls in this Python implementation.

### 3. BDR V2 remains the strongest Python prototype so far

At `lambda = 100`, BDR V2 retained 100% sampled fidelity and competitive latency. Its second-level `dict` remains the most efficient local engine tested in Python.

This also reinforces the earlier conclusion: the current Python BDR should be described as expected/amortized O(1), not strict worst-case O(1).

### 4. The next meaningful experiment should move the local engine out of Python

A fair test of the architectural hypothesis requires contiguous memory and compiled probing logic. Recommended next implementation:

- C++ or Rust core;
- fixed-width 64-bit `phi` and signature lanes;
- contiguous open-addressing tables per rho partition;
- Robin Hood or SwissTable-style metadata;
- explicit memory-per-record measurement;
- CPU cache-miss and branch-misprediction profiling;
- multi-threaded partition-local writes.

The hypothesis to test is no longer simply "is BDR faster than dict?". A stronger engineering question is whether resolutive partitioning provides advantages in cache locality, concurrency isolation, tail latency, or predictable memory behavior at large N.

## Reproduction

Run:

```bash
python benchmarks/bdr_v3_local_engines.py
```

Optional example:

```bash
python benchmarks/bdr_v3_local_engines.py --sizes 10000,100000,500000,1000000 --m 10000 --queries 5000
```

Output CSV:

`benchmarks/results/bdr_v3_local_engines.csv`

For publication-quality comparisons, record CPU model, RAM, OS, Python version, CPU governor/power mode, process affinity, and repeat the full experiment across multiple independent runs.
