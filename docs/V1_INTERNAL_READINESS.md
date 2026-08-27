# BDR v1 — Internal Readiness

Status: **internal candidate only**. This document is not a release or publication checklist.

Governance rule: no new public release, release-candidate publication, publication tag, or DOI before v1.0.

## Preferred internal architecture

- Database engine: `experimental/api_v86/src/database_v1.cpp`
- Index: `CompactIndex` by default
- Regression fallback: `ResolutiveIndex` with `BDR_V1_FALLBACK_INDEX`
- Snapshot format: BDR3, unchanged
- WAL format: BDW3, unchanged
- Checkpoint: streaming encoder
- Build: native CMake targets for candidate and fallback
- Installed package target: `bdr::bdr`
- Public C++ API candidate: `experimental/api_v86/include/bdr/database.hpp`
- Public API lock: `.github/v1-public-api.lock`
- v1 binary packaging policy: static CMake target; no cross-version shared-library ABI guarantee

## Evidence already closed

- compact-index contract parity against baseline and external oracle: PASS
- BDR3/BDW3 persistence parity: PASS
- torn final WAL recovery: PASS
- CRC rejection: PASS
- database crash recovery: PASS
- database concurrency at 1/4/8/16 writers: PASS
- exact-value verification after heavy churn: PASS
- 50M mutation scale campaign on the earlier baseline/experimental path: PASS
- 5M distinct-record cardinality campaign: PASS
- paired 1M candidate resource comparison: PASS
- paired 5M candidate resource comparison: PASS
- streaming BDR3 byte identity: PASS
- checkpoint crash-boundary, 12 failpoints: PASS on candidate and fallback
- generator/shim removal from v1 validation path: PASS
- native CMake build for candidate and fallback: PASS
- CTest contract suite for candidate and fallback: PASS
- cross-version BDR3/BDW3 compatibility: PASS in all four directions (baseline->candidate, candidate->baseline, fallback->candidate, candidate->fallback)
- repeated checkpoint/reopen churn: PASS on candidate and fallback
- checkpoint/reopen churn volume: 200 cycles, 2,000,000 operations per backend, 1,880,025 accepted mutations
- candidate checkpoint/reopen churn peak RSS: 26,664 KB, zero swap
- fallback checkpoint/reopen churn peak RSS: 27,956 KB, zero swap
- public C++ source API freeze candidate documented: PASS
- public header API-lock CI enforcement: PASS
- v1 ABI policy decision: PASS — source/API compatibility + static package; no shared-library binary ABI promise
- install/export through native CMake package: PASS
- clean external `find_package(bdr)` consumer linked through `bdr::bdr`: PASS
- installed-package write/checkpoint/close/reopen smoke: PASS
- candidate 1M fixed resource threshold gate: PASS
- candidate resource thresholds: RSS <= 330,000 KB, disk <= 55,000,000 bytes, records = 1,000,000, swaps = 0

## High-cardinality evidence

At 5M records in paired same-runner measurements before source cleanup, CompactIndex reduced RSS by about 26.8% versus the baseline index while keeping ingest throughput at least equivalent.

With streaming checkpoint enabled, paired 5M lifecycle testing showed lower RSS, faster reopen, equal persistent footprint, and no durability regression. Timing gains remain secondary evidence because hosted-runner I/O variance is significant; memory reduction and correctness gates are treated as the stronger signals.

At 1M records in the materialized candidate/fallback path, the candidate used 280,400 KB peak RSS versus 370,568 KB for fallback, while both produced 53,777,840 bytes of persistent data. This evidence is the basis for the current conservative resource gate.

## Gates still open before any future merge toward v1

1. Materialized long soak:
   - run the 50M mutation scale campaign directly against `database_v1.cpp`;
   - require exact oracle verification, reopen/checkpoint stability and no durability regression.
2. Final repository audit and release closure:
   - verify tests, docs and licenses;
   - update `CITATION.cff`, `.zenodo.json`, `pyproject.toml`, README/release metadata only when v1 is actually promoted;
   - preserve historical v0.2.0-rc1 metadata until that point;
   - ensure no premature public v1/DOI language exists.

## Non-goals at this stage

- no new disk format;
- no public release;
- no DOI;
- no removal of the preserved baseline implementation;
- no administrative UI work until the database reaches v1.0.
