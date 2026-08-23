# V100 Evidence Closure Summary

Status: **candidate = true**

This document records the observed evidence from GitHub Actions run `32650698844` for the experimental v0.2 API candidate.

## Validation result

All V100 steps completed successfully:

- V86 core build and contract tests: PASS
- 500,000-mutation soak: PASS
- C ABI v1 build and tests: PASS
- Exact ABI symbol set: PASS
- Installed Python wheel: PASS
- Python benchmark V89: completed
- Native market benchmark V90: completed
- V95 readiness audit: PASS (`candidate = true`)
- V97 release metadata audit: PASS
- V98 staging safety audit: PASS
- Final evidence manifest: `candidate = true`
- Evidence artifact upload: PASS

Artifact: `v100-pr-evidence-closure`
Artifact digest: `sha256:029a6952207d282f1783bcbaaff2051dd1f0f1d82b5547c035d030994be0c3ff`

## Frozen candidate hashes

- `experimental/api_v86/src/database.cpp`: `50fe581423029c95088af5fa0c05559f7c7ef6b99161e2bea9b1f3d9a28a01fb`
- `experimental/api_v86/src/resolutive_index.cpp`: `7b3de31f2a6b6b3c823159e6783356d34dfaad1814927787f73e91822cb7a290`
- `experimental/api_v87/include/bdr/bdr_c.h`: `4e5e1999f3cfe95449d656b9ff5f1e6979156fab254d38190784b13d99edeb0f`
- `experimental/api_v87/src/bdr_c.cpp`: `88d66072173c80fc192fddfcd14ae8848097d56da4b0ca5dba5e5986781366ca`
- `LICENSE`: `4de769cdef31a63539c85d09d6ac64c7d56f7b50087285e11ecb66f3d76798ff`
- `CITATION.cff`: `969a2a031a0b84c58278704cda24054faf480c6906037add6269dc6a68682175`

## V89 installed-Python benchmark

Observed results from the V100 run:

| Engine | Batch | PUT ops/s | GET ops/s | PUT p50 us | PUT p95 us | PUT p99 us | Errors |
|---|---:|---:|---:|---:|---:|---:|---:|
| BDR wheel | 1 | 3,975.87 | 96,762.79 | 219.33 | 293.40 | 483.61 | 0 |
| SQLite Python | 1 | 9,151.63 | 125,806.69 | 79.27 | 167.65 | 349.74 | 0 |
| BDR wheel | 128 | 83,802.72 | 100,675.11 | 394.15 | 544.68 | 1,246.27 | 0 |
| SQLite Python | 128 | 267,822.64 | 124,564.80 | 132.19 | 346.15 | 1,595.16 | 0 |

Interpretation: in this installed-Python benchmark SQLite is faster than the current BDR wheel for PUT and GET in both tested batch modes. This is a valid negative performance result and must be retained.

## V90 native market benchmark

Median observed throughput:

| Engine | Writers | Window | Median ops/s |
|---|---:|---:|---:|
| BDR | 1 | 128 | 196,257 |
| BDR | 4 | 128 | 452,287 |
| BDR | 8 | 128 | 550,300 |
| BDR | 16 | 128 | 526,503 |
| SQLite | 1 | 1 | 10,653.8 |
| LMDB | 1 | 1 | 4,353.27 |
| LevelDB | 1 | 1 | 4,623.13 |
| RocksDB | 1 | 1 | 4,702.57 |

**Important methodology limitation:** these rows do not use the same durability batching contract. BDR is measured with `window=128`, while SQLite/LMDB/LevelDB/RocksDB are measured with synchronous durability per operation (`window=1`). Therefore V90 must not be presented as a direct speed ranking or as evidence that BDR is N-times faster than those engines. It demonstrates BDR throughput under grouped durable writes and confirms that the native benchmark completes correctly against current market engines.

## Release interpretation

V100 closes the technical evidence needed to call this source tree an **API/ABI release candidate**. It does not by itself authorize publication, tagging, DOI creation, or changing the historical v0.1.0 citation metadata.

The proposed staging identity remains `v0.2.0-rc1` until release metadata is intentionally promoted.
