# BDR v1 — Internal Readiness

Status: **clean promotion candidate only**. This document is not a release or publication checklist.

Governance rule: no new public release, release-candidate publication, publication tag, or DOI before v1.0.

## Preferred v1 architecture

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
- materialized 50M mutation soak directly against `database_v1.cpp`: PASS
- clean selective-promotion branch CI: PASS for API lock, CMake/CTest, cross-compatibility, package consumer, 12-point crash boundary and paired resource gate

## Materialized 50M soak evidence

The materialized implementation `database_v1.cpp` completed the long soak with:

- operations: 50,000,000
- cycles: 5,000
- final records: 77,749
- elapsed wall time: 24:43.33
- peak RSS: 47,552 KB
- swaps: 0
- exit status: 0

The scale gate reported `PASS`, and the evidence artifact was uploaded successfully by GitHub Actions.

## Resource evidence

At 5M records in paired same-runner measurements before source cleanup, CompactIndex reduced RSS by about 26.8% versus the baseline index while keeping ingest throughput at least equivalent.

With streaming checkpoint enabled, paired 5M lifecycle testing showed lower RSS, faster reopen, equal persistent footprint, and no durability regression. Timing gains remain secondary evidence because hosted-runner I/O variance is significant; memory reduction and correctness gates are treated as the stronger signals.

The clean-promotion branch now uses a same-runner paired resource gate rather than a brittle absolute RSS limit. At 1M records in the latest clean run:

- candidate RSS: 279,156 KB
- fallback RSS: 347,256 KB
- candidate persistent footprint: 53,777,840 bytes
- fallback persistent footprint: 53,777,840 bytes
- candidate swaps: 0
- fallback swaps: 0
- records: exactly 1,000,000 in both paths

The candidate retained about a 19.6% RSS advantage in that run. The CI requires at least a 5% same-runner RSS advantage, identical disk footprint, exact cardinality and zero swap.

## Remaining gate before any future merge toward v1

Final repository/release closure only:

- verify the clean promotion diff remains limited to the proven v1 subset;
- keep `CITATION.cff`, `.zenodo.json`, `pyproject.toml` and public README/release metadata at the currently published v0.2.0-rc1 state until actual v1 promotion;
- update version/release metadata only in the final v1 promotion step;
- ensure no premature public v1/DOI language exists;
- do not merge into `main` until the clean promotion review is explicitly approved.

## Non-goals at this stage

- no new disk format;
- no public release before v1;
- no DOI before v1;
- no removal of the preserved baseline implementation;
- no administrative UI work until the database reaches v1.0.
