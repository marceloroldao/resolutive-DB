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

## Evidence already closed

- compact-index contract parity against baseline and external oracle: PASS
- BDR3/BDW3 persistence parity: PASS
- torn final WAL recovery: PASS
- CRC rejection: PASS
- database crash recovery: PASS
- database concurrency at 1/4/8/16 writers: PASS
- exact-value verification after heavy churn: PASS
- 50M mutation scale campaign: PASS
- 5M distinct-record cardinality campaign: PASS
- paired 1M candidate resource comparison: PASS
- paired 5M candidate resource comparison: PASS
- streaming BDR3 byte identity: PASS
- checkpoint crash-boundary, 12 failpoints: PASS on candidate and fallback
- generator/shim removal from v1 validation path: PASS
- native CMake build for candidate and fallback: PASS
- CTest contract suite for candidate and fallback: PASS

## High-cardinality evidence

At 5M records in paired same-runner measurements before source cleanup, CompactIndex reduced RSS by about 26.8% versus the baseline index while keeping ingest throughput at least equivalent.

With streaming checkpoint enabled, paired 5M lifecycle testing showed lower RSS, faster reopen, equal persistent footprint, and no durability regression. Timing gains remain secondary evidence because hosted-runner I/O variance is significant; memory reduction and correctness gates are treated as the stronger signals.

## Required gates before any future merge toward v1

1. Cross-version file compatibility:
   - baseline-created BDR3/BDW3 -> v1 candidate read/recover;
   - v1 candidate-created BDR3/BDW3 -> baseline/fallback read/recover;
   - deletes, updates, checkpoints and sequence numbers must remain exact.
2. Long soak on the materialized `database_v1.cpp`, not a generated source.
3. Repeated checkpoint/reopen cycles under churn on candidate and fallback.
4. API/ABI review and explicit decision about the v1 public ABI surface.
5. Build/install/package validation from the native CMake path.
6. Final resource regression gate with fixed acceptance thresholds.
7. Final repository audit: tests, docs, licenses, metadata and no experimental publication language.

## Non-goals at this stage

- no new disk format;
- no public release;
- no DOI;
- no removal of the preserved baseline implementation;
- no administrative UI work until the database reaches v1.0.
