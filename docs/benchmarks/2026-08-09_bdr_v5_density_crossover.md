# BDR V5 Density Crossover Benchmark — 2026-08-09

## Objective
Measure the crossover between the memory-optimized BDR V5 Compact engine and the V4 Robin Hood local engine as density `lambda = N/M` increases.

## Method
- N = 500,000 records fixed
- phase buckets = 65,536
- M varied from 500,000 down to 250
- lambda therefore varied from 1 to 2,000
- 250,000 lookup operations per configuration
- first 5,000 operations sampled for p50/p99
- deterministic PRNG seed = 42
- compiler command: `g++ -O3 -std=c++17 benchmarks/bdr_v5_density_crossover.cpp -o bdr_cross`

## Results from this execution

| M | lambda | Engine | Build (s) | Mops/s | p50 us | p99 us | bytes/record |
|---:|---:|---|---:|---:|---:|---:|---:|
| 500000 | 1 | Compact | 0.1535 | 1.4227 | 1.148 | 2.077 | 40.00 |
| 500000 | 1 | Robin | 0.1606 | 1.5774 | 0.610 | 1.185 | 416.00 |
| 100000 | 5 | Compact | 0.0591 | 1.6873 | 0.491 | 1.051 | 20.80 |
| 100000 | 5 | Robin | 0.1481 | 1.7068 | 0.552 | 1.102 | 160.00 |
| 50000 | 10 | Compact | 0.0468 | 1.7320 | 0.459 | 0.967 | 18.40 |
| 50000 | 10 | Robin | 0.1164 | 1.8311 | 0.514 | 1.005 | 156.80 |
| 10000 | 50 | Compact | 0.0483 | 1.7316 | 0.585 | 1.246 | 16.48 |
| 10000 | 50 | Robin | 0.0932 | 2.1370 | 0.449 | 0.941 | 62.08 |
| 5000 | 100 | Compact | 0.0486 | 1.8865 | 0.586 | 1.230 | 16.24 |
| 5000 | 100 | Robin | 0.0884 | 2.1011 | 0.436 | 0.948 | 61.76 |
| 1000 | 500 | Compact | 0.0527 | 1.7161 | 0.624 | 1.376 | 16.05 |
| 1000 | 500 | Robin | 0.0903 | 1.9216 | 0.447 | 0.960 | 98.37 |
| 500 | 1000 | Compact | 0.0540 | 1.6417 | 0.586 | 1.388 | 16.02 |
| 500 | 1000 | Robin | 0.0850 | 2.2109 | 0.433 | 0.914 | 98.34 |
| 250 | 2000 | Compact | 0.0548 | 2.0519 | 0.595 | 1.414 | 16.01 |
| 250 | 2000 | Robin | 0.0869 | 2.2145 | 0.430 | 0.944 | 98.32 |

## Interpretation
The compact engine dominates memory use by a wide margin across the entire tested range. Once M is not extremely large, its index converges toward about 16 bytes per record in this implementation.

Robin Hood generally wins lookup throughput and tail latency as density rises. The performance separation becomes practically meaningful around lambda ~= 50 in this run. At lambda 50, Robin Hood delivered about 23% more throughput and a lower p99. At lambda 100 and above, Robin Hood continued to provide lower p50/p99 in most cases.

At low densities (lambda 5–10), the two engines are close enough that Compact is attractive because its memory advantage is very large. Therefore a provisional adaptive policy is:

- `lambda < 50`: prefer Compact for read-mostly partitions, especially when memory efficiency matters.
- `lambda >= 50`: consider Robin Hood when latency/throughput is the primary objective.
- write-heavy/concurrent partitions remain candidates for Robin Hood regardless of density because Compact is a static/batch-oriented layout in this experiment.

This threshold is empirical, hardware- and workload-dependent. It is not a universal constant and must be calibrated during deployment or startup.

## Next experiment
Build an adaptive hybrid engine where each rho_R partition selects its local representation from measured occupancy and workload characteristics, rather than using one engine globally. The important comparison will be Hybrid vs pure Compact vs pure Robin Hood under skewed occupancy, Zipf/hot-key access, and mixed read/write workloads.
