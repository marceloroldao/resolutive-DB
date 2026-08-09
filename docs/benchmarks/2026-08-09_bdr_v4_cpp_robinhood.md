# BDR V4 — Compiled C++ Robin Hood benchmark

Date: 2026-08-09

## Goal

Measure whether the resolutive partitioning architecture benefits from a compiled, contiguous-memory local engine instead of Python-level dictionaries. The BDR V4 benchmark uses:

- first-level direct partitioning by `rho_R`;
- local Robin Hood open addressing per partition;
- the same deterministic encoder for both BDR V4 and the flat `std::unordered_map` control;
- hit and miss latency;
- median and p99;
- insertion time;
- sampled fidelity.

This benchmark does **not** use SHA-256. It intentionally uses the same lightweight deterministic FNV-1a + 64-bit mixing encoder for both competitors so the experiment isolates layout/index structure rather than cryptographic hashing cost.

## Build

```bash
g++ -O3 -std=c++17 -march=native benchmarks/bdr_v4_cpp_robinhood.cpp -o bdr_v4
./bdr_v4
```

The default checked-in sizes are 10,000, 100,000 and 300,000 records so the test remains executable on modest hardware. Larger systems may extend the `sizes` vector to 1M, 10M or more.

## Benchmark integrity note

An initial local run produced unrealistically low values (~17 ns). Investigation showed that with `-O3` the compiler could optimize away part of the lookup path because the query result was not consumed. The benchmark was corrected by accumulating each lookup result into a `volatile` sink.

Only the corrected measurements below are retained as valid results.

## Environment-local results

Fixed parameters:

- `M = 10,000` rho partitions
- `phi_buckets = 65,536`
- 5,000 hit queries
- 5,000 miss queries
- fixed random seed 42

| N | lambda=N/M | BDR insert (s) | flat insert (s) | BDR hit median (us) | BDR hit p99 (us) | flat hit median (us) | flat hit p99 (us) | BDR miss median (us) | BDR miss p99 (us) | flat miss median (us) | flat miss p99 (us) | fidelity |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10,000 | 1 | 0.00170 | 0.00162 | 0.115 | 0.393 | 0.114 | 0.517 | 0.164 | 0.525 | 0.088 | 0.297 | 100% |
| 100,000 | 10 | 0.01748 | 0.02344 | 0.251 | 0.661 | 0.227 | 0.647 | 0.223 | 0.604 | 0.158 | 0.643 | 100% |
| 300,000 | 30 | 0.05251 | 0.07707 | 0.299 | 0.706 | 0.298 | 0.763 | 0.239 | 0.670 | 0.177 | 0.722 | 100% |

## Interpretation

The compiled result is materially different from the Python V3 experiments.

### 1. Insertion becomes competitive

At 100k and 300k records, BDR V4 inserted faster than the flat `std::unordered_map` control in this run:

- 100k: ~0.0175 s BDR vs ~0.0234 s flat
- 300k: ~0.0525 s BDR vs ~0.0771 s flat

This is the first benchmark in the project where the partitioned BDR layout showed a measurable absolute performance advantage over a conventional flat hash structure for a core operation.

This result must be treated as preliminary because the two structures do not have identical memory policies and the BDR currently over-allocates local partition capacity conservatively.

### 2. Hit latency is essentially tied at lambda=30

At 300k records:

- BDR hit median: ~0.299 us
- flat hash hit median: ~0.298 us

The difference is negligible in this run.

### 3. BDR p99 hit latency is slightly better at 300k

At 300k:

- BDR hit p99: ~0.706 us
- flat hash hit p99: ~0.763 us

This is consistent with the hypothesis that local Robin Hood tables may help tail behavior by bounding probe-distance variance within partitions. It is not yet sufficient evidence of a general advantage.

### 4. Misses still favor the flat hash table

At 300k:

- BDR miss median: ~0.239 us
- flat miss median: ~0.177 us

The BDR pays the extra partition lookup and local probing path even when no record exists.

### 5. Fidelity remained 100%

All sampled BDR lookups reconstructed the expected values exactly for the tested address width.

## Important limitations

1. This is a microbenchmark, not a full database benchmark.
2. Persistence, WAL, transactions, crash recovery, range scans, secondary indexes and concurrent writers are not yet included.
3. `std::unordered_map` is only one conventional hash-map baseline. Future tests should add SwissTable-style maps and Robin Hood flat maps.
4. The BDR allocates a fixed local table size per rho partition based on expected average density with a conservative safety factor. That favors predictability but can waste memory.
5. The local engine currently cannot resize an individual partition. A pathological distribution could fill one partition and cause nontermination. Production code must add per-partition growth or overflow handling.
6. The benchmark encoder is deterministic but is not cryptographic SHA-256. That choice is intentional for isolating data-structure cost.

## Engineering conclusion

The C++ experiment provides the first evidence that the BDR partitioning idea can be competitive with a conventional flat hash map once Python overhead is removed. The strongest measured signal is currently insertion throughput and, secondarily, hit p99 at the highest tested density.

It still does not establish a new asymptotic complexity class. Both BDR V4 and the flat hash baseline remain expected/amortized O(1) hash-based structures.

The next meaningful test should therefore focus on properties where partitioning can plausibly offer an architectural advantage:

- per-partition memory locality;
- multithreaded inserts/reads without a global lock;
- p99/p99.9 under contention;
- NUMA locality;
- memory bytes per record;
- partition-local resizing;
- comparison with high-performance flat hash maps rather than only `std::unordered_map`.
