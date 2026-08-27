# BDR v1.0.0 — Draft Release Notes

Status: **DRAFT / NOT TAGGED / NOT PUBLISHED**

This document prepares the first stable BDR release. It does not create a release, tag, DOI, or publication.

## Core

- `CompactIndex` becomes the preferred v1 internal index.
- Legacy `ResolutiveIndex` remains available internally as a regression fallback.
- Public C++ source API is frozen through `database.hpp` and CI lock.
- Stable CMake consumer target: `bdr::bdr`.
- Only the public `database.hpp` header is installed; index implementations remain private.

## Persistence and recovery

- BDR3 snapshot format preserved.
- BDW3 WAL format preserved.
- Streaming checkpoint path validated byte-for-byte against the previous buffered representation.
- Torn final WAL recovery preserved.
- Checkpoint crash-boundary validation passes across 12 deterministic failpoints.
- Four-direction cross-version compatibility passes between baseline, v1 candidate, and fallback.

## Scale and robustness evidence

- 50,000,000-operation materialized soak: PASS.
- 200 checkpoint/reopen cycles with 2,000,000 operations: PASS on candidate and fallback.
- 5,000,000-record paired benchmark demonstrated lower RSS for CompactIndex with no material throughput regression in same-runner comparisons.
- Package-consumer installation/reopen test: PASS.
- BDR CI: PASS.
- V99 evidence closure: PASS.
- V100 evidence closure: PASS.

## Compatibility policy

- v1 preserves BDR3/BDW3 persistence compatibility with the current published baseline.
- v1 guarantees the frozen C++ source API documented in `docs/V1_PUBLIC_API.md`.
- v1 does **not** promise shared-library ABI compatibility across future releases; the supported distribution contract is the static CMake package target `bdr::bdr`.

## Release governance

Before publication, all of the following must be true:

1. finalization branch CI is green;
2. final `main` tree is validated;
3. README, CHANGELOG, CITATION.cff, `.zenodo.json`, and package version metadata are updated together;
4. no DOI is invented or predeclared;
5. tag/release publication occurs only after explicit final approval.
