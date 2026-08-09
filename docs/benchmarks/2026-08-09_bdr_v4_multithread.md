# BDR V4 — Multithread + Memory Benchmark

Date: 2026-08-09

## Objective

Measure whether resolutive partitioning by `rho_R` improves concurrency when each partition owns an independent Robin Hood table and independent `std::shared_mutex`.

The control is a single `std::unordered_map<uint64_t,uint64_t>` protected by one global `std::shared_mutex`. Both engines use the same encoder and the same key set.

This benchmark is intended to test a concrete architecture, not to establish universal superiority over modern concurrent hash maps.

## Reproduction

Compile:

```bash
g++ -O3 -std=c++17 -pthread benchmarks/bdr_v4_multithread.cpp -o bdr_v4_multithread
```

Run:

```bash
./bdr_v4_multithread
```

The program emits CSV to stdout.

## Workload

- N = 300,000 records
- M = 10,000 rho partitions
- lambda = 30 records/partition on average
- phi buckets = 65,536
- 50,000 operations per thread
- thread counts = 1, 2, 4, 8, 16
- modes:
  - read: 100% lookups
  - mixed90r10w: 90% lookups / 10% updates
- latency sample: first 5,000 operations per thread
- deterministic RNG seed: 42

## Execution environment

- OS: Linux x86_64, kernel 6.18.35
- Compiler: g++ 14.2.0
- CPU: Intel Xeon Platinum 8370C @ 2.80 GHz
- visible logical CPUs: 5
- threads/core: 1
- L3 cache: 48 MiB

Important: 8- and 16-thread results oversubscribe the 5 CPUs visible to the container. They remain useful as contention/oversubscription observations, but they are not clean physical-core scaling measurements.

## Results — read-only

| Engine | Threads | Mops/s | p50 us | p95 us | p99 us | p99.9 us |
|---|---:|---:|---:|---:|---:|---:|
| BDR | 1 | 1.629 | 1.016 | 1.731 | 2.089 | 3.386 |
| FlatHash | 1 | 1.863 | 0.825 | 1.545 | 1.952 | 4.419 |
| BDR | 2 | 4.706 | 0.524 | 0.860 | 1.057 | 3.577 |
| FlatHash | 2 | 3.702 | 0.727 | 1.142 | 1.415 | 11.215 |
| BDR | 4 | 8.109 | 0.508 | 0.813 | 1.009 | 6.423 |
| FlatHash | 4 | 4.853 | 0.787 | 1.205 | 1.486 | 9.547 |
| BDR | 8 | 8.243 | 0.503 | 0.814 | 1.000 | 8.132 |
| FlatHash | 8 | 5.479 | 0.825 | 1.273 | 1.595 | 14.915 |
| BDR | 16 | 9.186 | 0.459 | 0.758 | 0.949 | 8.373 |
| FlatHash | 16 | 5.149 | 0.809 | 1.240 | 1.528 | 10.860 |

## Results — mixed 90% read / 10% write

| Engine | Threads | Mops/s | p50 us | p95 us | p99 us | p99.9 us |
|---|---:|---:|---:|---:|---:|---:|
| BDR | 1 | 2.506 | 0.555 | 0.920 | 1.112 | 5.959 |
| FlatHash | 1 | 1.715 | 0.712 | 1.155 | 1.436 | 7.307 |
| BDR | 2 | 3.436 | 0.568 | 0.919 | 1.124 | 1.425 |
| FlatHash | 2 | 0.812 | 0.831 | 11.726 | 19.438 | 95.243 |
| BDR | 4 | 7.268 | 0.514 | 0.873 | 1.073 | 6.855 |
| FlatHash | 4 | 0.675 | 0.889 | 23.651 | 76.243 | 257.438 |
| BDR | 8 | 7.858 | 0.496 | 0.822 | 1.018 | 2.825 |
| FlatHash | 8 | 1.193 | 0.736 | 15.338 | 161.669 | 2866.790 |
| BDR | 16 | 8.099 | 0.484 | 0.789 | 0.986 | 6.507 |
| FlatHash | 16 | 1.181 | 0.767 | 16.119 | 472.702 | 4614.390 |

## Approximate index memory

The benchmark reports index-structure memory, not complete process RSS and not payload/string storage.

- BDR fixed local-slot allocation: 62,320,000 bytes, ~207.7 bytes/record
- Flat `unordered_map` approximation: 14,471,224 bytes, ~48.2 bytes/record

The FlatHash estimate includes bucket pointers and a rough node estimate. Exact allocator overhead is implementation-specific, so this is not a byte-perfect comparison.

The BDR number is much less ambiguous because the local Robin Hood tables use fixed `std::vector<RHSlot>` arrays. The current V4 is therefore clearly over-provisioned in memory.

## Interpretation

### Read-only

With one thread, the flat map is faster in median latency and throughput. As concurrency rises, the partitioned BDR becomes faster in this benchmark. At 4 threads:

- BDR: 8.109 Mops/s
- FlatHash: 4.853 Mops/s

At 16 software threads (oversubscribed on this host):

- BDR: 9.186 Mops/s
- FlatHash: 5.149 Mops/s

This result supports the narrower claim that partition-local synchronization can reduce contention relative to one globally synchronized table.

### Mixed reads/writes

The global-lock control degrades severely as writers appear. This is expected: every write takes the single map's unique lock and conflicts with all readers/writers.

BDR writes lock only one rho partition. Because requests are distributed over 10,000 partitions, unrelated operations usually do not contend on the same mutex.

This is an architectural advantage over this specific globally locked baseline.

It is NOT evidence that BDR is faster than production-grade concurrent hash maps such as sharded maps, SwissTable-derived concurrent structures, TBB `concurrent_hash_map`, Folly F14 variants, or specialized lock-free maps. Those are required future baselines.

## Main negative result

Memory consumption is currently poor. The fixed-capacity strategy uses about 4.3x the estimated index memory per record of the flat baseline in this configuration.

Therefore V4 currently trades memory for partition independence and predictable local capacity.

## Methodological caveats

1. Only one benchmark execution is recorded here. Future runs should report medians across independent repetitions.
2. The container exposes only five CPUs, so 8/16-thread rows are oversubscribed.
3. Thread scheduling and virtualization add noise, especially to p99.9.
4. `std::unordered_map + shared_mutex` is intentionally simple and is not a state-of-the-art concurrent hash map.
5. BDR allocates fixed local capacity conservatively; this strongly affects memory results.
6. Encoder cost is included end-to-end for both engines.
7. Updates rewrite existing keys; this benchmark does not exercise concurrent structural growth/rehash of the BDR tables.

## Engineering conclusion

V4 provides the first reproducible evidence in this project that rho partitioning can have a practical concurrency benefit, specifically by reducing synchronization scope. The current evidence supports:

> Partitioned BDR V4 scales better than a single globally synchronized `std::unordered_map` under this workload and host.

It does not yet support:

> BDR is universally faster than modern databases or modern concurrent hash maps.

The largest current weakness is memory overhead.

## Next experiments

1. Dynamic per-partition capacity instead of fixed 4x provisioning.
2. Compare against a sharded `unordered_map` with the same number of shards. This is critical because it tests whether the observed benefit comes from the resolutive address decomposition or simply from sharding.
3. Compare against one high-quality concurrent hash table implementation.
4. Repeat on a physical 8/16/32-core host without CPU oversubscription.
5. Record process RSS and allocator statistics before/after each engine independently.
6. Separate encoder cost from pure index lookup cost.
7. Test skewed/Zipfian rho distributions to identify hot-partition collapse.
