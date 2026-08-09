# BDR V4 — Phase-aware core affinity benchmark

Date: 2026-08-09

## Goal

Test the hypothesis suggested during BDR V4 development: assign different phase subsets (`phi`) to different worker/core lanes so that work is routed using phase affinity rather than only a conventional hash shard.

This benchmark isolates the **scheduling/sharding effect**. It does not claim that the phase scheduler alone is a new indexing primitive.

## Compared architectures

1. `GlobalDB`: one `std::unordered_map` guarded by one `std::shared_mutex`.
2. `Generic`: sharded `std::unordered_map`; shard chosen by a conventional mixed hash.
3. `Phase Stripe`: shard/core chosen by `phi % thread_count`.
4. `Phase Band`: the quantized phase interval is divided into contiguous bands, one per worker.

All variants use the same deterministic encoder and same 300,000-key dataset.

## Build

```bash
g++ -O3 -std=c++20 -pthread benchmarks/bdr_v4_phase_affinity.cpp -o bdr_v4_phase_affinity
./bdr_v4_phase_affinity
```

## Method

- N = 300,000 records
- rho regions M = 10,000
- phase buckets = 65,536
- 50,000 operations per thread per run
- thread counts = 1, 2, 4, 5
- workloads: read-only and 90% read / 10% write
- three repetitions per point; reported number is median throughput
- output unit = million operations/second (Mops/s)

## Results measured in this session

| Threads | Workload | Global | Generic shard | Phase stripe | Phase band |
|---:|---|---:|---:|---:|---:|
| 1 | read | 1.465 | **2.003** | 1.867 | 1.876 |
| 1 | mixed 90/10 | 1.849 | 1.901 | **1.921** | 1.902 |
| 2 | read | 3.463 | 3.495 | **3.553** | 3.493 |
| 2 | mixed 90/10 | 0.761 | 3.259 | **3.410** | 3.173 |
| 4 | read | 5.264 | 5.911 | **6.333** | 6.120 |
| 4 | mixed 90/10 | 0.635 | 5.525 | **5.764** | 5.451 |
| 5 | read | 5.865 | **6.980** | 6.020 | 6.872 |
| 5 | mixed 90/10 | 0.841 | 6.295 | **6.981** | 6.870 |

## Observations

The phase-aware scheduler is not universally faster, but it produced repeatable improvements in several concurrent cases.

At four threads/read-only, phase stripe reached 6.333 Mops/s versus 5.911 Mops/s for generic sharding, about 7.1% higher throughput.

At five threads/mixed 90/10, phase stripe reached 6.981 Mops/s versus 6.295 Mops/s for generic sharding, about 10.9% higher throughput.

At five threads/read-only, generic sharding remained faster than phase stripe. Phase band was much closer to the generic result. Therefore the current evidence does **not** support a universal claim that phase routing is superior.

## Interpretation

The experiment supports a narrower engineering hypothesis:

> A phase coordinate can be used as an affinity signal to partition concurrent work, and some phase-to-worker mappings can reduce contention or improve locality relative to a generic shard selector under specific workloads.

The result may come from distribution balance, lock ownership, cache reuse, or reduced cross-shard access. More instrumentation is required before attributing the gain specifically to cache locality.

## Important limitation

The phase-aware variants and the generic sharded baseline all use `std::unordered_map` internally in this scheduling experiment. Therefore this test isolates **routing/affinity**, not the Robin Hood local-table implementation used by the previous BDR V4 benchmark.

The execution environment exposed roughly five CPU execution units to the process. Results above five threads were intentionally not used in this refined run.

## Next experiment

Integrate phase affinity with the compiled BDR V4 Robin Hood partitions, then compare against a generic sharded Robin Hood or high-quality concurrent hash baseline. Add:

- explicit CPU affinity/pinning where permitted;
- p50/p95/p99/p99.9 latency;
- per-core operation counts and phase-distribution imbalance;
- cache-miss and branch-miss counters where `perf` is available;
- NUMA-aware placement on larger systems;
- dynamic phase-to-core assignment to avoid hot phases.

A promising scheduler rule to test is a two-dimensional route:

```text
(rho_R, phi) -> partition -> phase lane -> worker/core
```

rather than using `phi` alone.
