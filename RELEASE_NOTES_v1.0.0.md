# BDR v1.0.0 — Release Notes

Status: **PUBLICATION READY / TAG PENDING**

BDR v1.0.0 is the first stable-engine line of the Resolutive Database Engine. The technical tree has completed final validation and is frozen for publication. The release becomes public only when the v1.0.0 tag/GitHub Release is created from the validated commit.

## Core

- `CompactIndex` is the preferred v1 internal index.
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

## Performance claims

BDR v1.0.0 does not claim universal performance superiority over existing database engines. Benchmark results remain workload-specific and environment-specific. The project does not claim strict worst-case O(1) complexity for the complete database engine.

## Publication metadata

- Version: `1.0.0`.
- Software DOI: intentionally not predeclared. It must be added only after the publication service returns the real identifier.
- Scientific preprint DOI: `10.5281/zenodo.21937842`.
- Software license: BDR Academic and Non-Commercial Research License v1.0.

## Freeze rule

Any code change after this publication-ready validation invalidates the frozen release tree and requires rerunning the relevant technical gates before tagging v1.0.0.
