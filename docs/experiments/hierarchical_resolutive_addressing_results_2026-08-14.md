# Hierarchical Resolutive Addressing — GitHub Actions Results

**Date:** 2026-08-14  
**Branch:** `experiment/hierarchical-resolutive-addressing`  
**Workflow run:** `31836027990`  
**Artifact:** `hierarchical-resolutive-addressing-results` (`sha256:ef38ad9844761d464ac56ad978abdfbf6dc9c51d97e671bae5303c571266dad7`)  
**Seed:** `0xB0D2A018`

## Environment

- Ubuntu 24.04 hosted runner
- kernel `6.17.0-1022-azure`
- AMD EPYC 9V74
- 4 logical CPUs visible
- g++ 13.3.0

Main matrix: `N=100000`, `Q=50000`, `M=2048`, three repetitions. Values below are medians across the three raw CSV files.

## Sequential/uniform workload

| Mode | mean us | p99 us | throughput ops/s | approx bytes/record | avg candidates |
|---|---:|---:|---:|---:|---:|
| rho+fp | **0.124** | **0.231** | 5,960,880 | **28.59** | 49.77 |
| rho+phi+fp | 0.241 | 0.641 | 3,535,860 | 64.58 | 1.001 |
| rho+phi+theta+fp | 0.210 | 0.531 | 3,960,240 | 64.59 | 1.000 |
| fixed Z1/Z2/Z3 | 0.268 | 0.712 | 3,224,280 | 100.58 | 1.000 |
| fixed Z1/Z2/Z3/Z4 | 0.284 | 0.731 | 3,057,750 | 136.58 | 1.000 |
| adaptive no memory | 0.272 | 0.681 | 3,227,060 | 100.58 | 1.000 |
| adaptive metadata | 0.220 | 0.590 | 3,871,080 | 64.58 | 1.001 |
| simple cache | 0.129 | 0.251 | 5,995,290 | 29.57 | 47.86 |
| frequency counter | 0.123 | 0.240 | **6,207,090** | 28.75 | 49.77 |
| adaptive metadata + cache | 0.219 | 0.561 | 3,865,310 | 65.56 | 0.963 |

**Negative result:** under a well-dispersed uniform workload, the simplest `rho+fingerprint` path is better than hierarchical refinement on mean latency, p99 and memory. Therefore `phi/theta` must not be forced globally.

## Zipf-like / hot-key workload

| Mode | mean us | p99 us | throughput ops/s | cache hit % |
|---|---:|---:|---:|---:|
| rho+fp | **0.103** | **0.230** | **7,054,570** | 0 |
| adaptive metadata | 0.184 | 0.521 | 4,495,280 | 0 |
| simple cache | 0.112 | 0.240 | 6,656,360 | 27.42 |
| adaptive metadata + cache | 0.180 | 0.511 | 4,577,520 | 27.42 |

**Result:** in this implementation, a small direct-mapped cache did not beat the already-cheap `rho+fp` leaf scan despite a ~27% hit rate. The result does not justify structural memory as a replacement for cache, nor cache as a replacement for structural refinement.

## Hot-primary-partition workload

| Mode | mean us | p99 us | throughput ops/s | approx bytes/record | avg candidates |
|---|---:|---:|---:|---:|---:|
| rho+fp | 0.451 | 0.842 | 2,042,550 | **28.59** | **1563.4** |
| rho+phi+fp | 0.214 | 0.541 | 3,950,530 | 64.22 | 1.023 |
| rho+phi+theta+fp | **0.208** | **0.501** | **4,054,350** | 64.59 | 1.000 |
| fixed Z1/Z2/Z3 | 0.250 | 0.621 | 3,474,160 | 100.22 | 1.000 |
| adaptive no memory | 0.235 | 0.581 | 3,655,610 | 100.22 | 1.000 |
| adaptive metadata | 0.223 | 0.551 | 3,829,050 | 64.22 | 1.023 |
| adaptive metadata + cache | 0.214 | 0.521 | 3,963,800 | 65.20 | 0.985 |

Relative to `rho+fp`, `rho+phi+theta+fp` reduces median mean lookup latency by about **54%** and increases throughput by about **98%** in this structural-skew workload, at roughly 2.26x the estimated index bytes/record.

This supports the narrow engineering claim that secondary/tertiary refinement can disperse a concentrated primary partition.

## Collapse test — only 64 primary buckets

Single recorded collapse run (`N=100000`, `M=64`):

| Mode | mean us | p99 us | throughput ops/s | bytes/record | avg candidates |
|---|---:|---:|---:|---:|---:|
| rho+fp | 0.457 | 0.881 | 1,992,870 | **24.14** | 1563.18 |
| rho+phi+fp | 0.273 | 0.691 | 3,168,530 | 59.77 | 1.024 |
| rho+phi+theta+fp | 0.239 | 0.601 | 3,558,970 | 60.14 | 1.000 |
| adaptive metadata | **0.201** | **0.481** | **4,101,650** | 59.77 | 1.024 |
| fixed Z1/Z2/Z3/Z4 | 0.330 | 0.802 | 2,671,120 | 131.77 | 1.000 |

The adaptive metadata policy is strongest in this forced-collapse case. This is evidence for conditional refinement, not for using maximum depth everywhere.

## Fingerprint sweep

At 500,000 generated fingerprints:

| fingerprint | packed bytes | observed duplicate fingerprints | sort s | binary lookup ops/s |
|---|---:|---:|---:|---:|
| 64 bit | 8 | 0 | 0.0344 | 7,075,470 |
| 96 bit | 12 | 0 | 0.0369 | 6,792,980 |
| 128 bit | 16 | 0 | 0.0384 | 6,607,550 |

No collision was observed at this scale. This does not prove 64-bit safety at larger populations. The wider guards cost memory and modest comparison throughput, so the final width must be selected from target-scale collision risk, not benchmark speed alone.

## Hardware-counter limitation

`perf stat` was attempted for cycles, instructions, cache misses, branches and branch misses. GitHub's hosted runner rejected access because `perf_event_paranoid=4`. Therefore this experiment makes **no measured cache-locality claim** yet.

## Interpretation

### Preserved components

- `rho` as a cheap primary partition;
- exact fingerprint guard;
- conditional `phi`/`theta` refinement when measured structural concentration is high;
- compact per-partition metadata describing preferred depth;
- bounded routing depth as a routing property only.

### Components not justified as universal

- always using `phi`;
- always using `theta`;
- putting `f_nu` in the operational address;
- fixed Z1/Z2/Z3 for every partition;
- Z4 as default;
- calling structural metadata a cache;
- worst-case O(1) claims.

## Novelty status

The mechanism remains operationally close to hierarchical/multi-level hashing plus adaptive partition refinement. The present results show a useful conditional policy but do not yet establish a new data-structure primitive distinct from known adaptive hashing/sharding families.

## Decision

**Do not merge.**

The signal is strong enough to continue the experiment, especially under hot partitions and forced collapse, but direct matched-hash comparisons against `std::unordered_map`, the project's Robin Hood implementation and a mature sharded/flat-hash control are still required before recommending incorporation into a future BDR release.
