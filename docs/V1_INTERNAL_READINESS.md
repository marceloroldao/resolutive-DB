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
- Public C++ API candidate: `experimental/api_v86/include/bdr/database.hpp`
- Public API lock: `.github/v1-public-api.lock`

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

## High-cardinality evidence

At 5M records in paired same-runner measurements before source cleanup, CompactIndex reduced RSS by about 26.8% versus the baseline index while keeping ingest throughput at least equivalent.

With streaming checkpoint enabled, paired 5M lifecycle testing showed lower RSS, faster reopen, equal persistent footprint, and no durability regression. Timing gains remain secondary evidence because hosted-runner I/O variance is significant; memory reduction and correctness gates are treated as the stronger signals.

## Gates still open before any future merge toward v1

1. Materialized long soak:
   - run the 50M mutation scale campaign directly against `database_v1.cpp`;
   - require exact oracle verification, reopen/checkpoint stability and no durability regression.
2. Binary ABI decision:
   - the C++ source/API surface is frozen and CI-locked;
   - stable shared-library ABI is not yet claimed;
   - decide shared/static packaging, symbol visibility and versioning policy before any ABI guarantee.
3. Build/install/package validation from the native CMake path.
4. Final resource regression gate with fixed acceptance thresholds.
5. Final repository audit: tests, docs, licenses, metadata and no experimental publication language.

## Non-goals at this stage

- no new disk format;
- no public release;
- no DOI;
- no removal of the preserved baseline implementation;
- no administrative UI work until the database reaches v1.0.
