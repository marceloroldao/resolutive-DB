# BDR v0.2.0-rc1 — Release Candidate Notes

**Published release candidate. Experimental; not production-ready.**

BDR v0.2.0-rc1 is the first API-capable release-candidate line of the Banco de Dados Resolutivo (BDR) / Resolutive Database Engine.

Software DOI: **10.5281/zenodo.22074886**

## Highlights

- Consolidated C++ database core.
- Deterministic `rho + local Robin Hood + fingerprint` addressing path.
- C++ API for open/get/put/delete/wait/sync/checkpoint/close.
- Frozen C ABI v1 for foreign-language integration.
- Python package `bdr-native` at version `0.2.0rc1`, consuming the C ABI v1.
- CMake install package for external C/C++ consumers.
- BDW3 write-ahead log with header and record integrity checks.
- BDR3 checkpoints with CRC validation.
- Streaming snapshot recovery.
- Torn-tail repair for incomplete final WAL frames.
- Exclusive process-level database lock.
- Writer I/O errors propagated through the API instead of terminating the host process.
- Multiwriter, checkpoint, close/submit, crash, soak and compatibility validation gates.
- Native comparison framework against SQLite, LMDB, LevelDB and RocksDB.

## Validation evidence

The V100 PR Evidence Closure completed successfully and produced `candidate: true`.

Validated items include:

- V86 core contract: PASS
- 500,000-mutation soak: PASS
- C ABI v1 contract: PASS
- exact ABI symbol set: PASS
- installed Python wheel: PASS
- Python benchmark: completed
- native market benchmark: completed
- readiness audit V95: PASS
- metadata audit V97: PASS
- staging audit V98: PASS

Evidence details and SHA-256 hashes are recorded in `docs/release/v0.2.0-rc1-evidence.md`.

## Performance interpretation

This release candidate does **not** claim universal performance superiority over existing databases.

In the V100 installed-Python benchmark, SQLite was faster than BDR in the tested workloads. For example:

| Configuration | BDR wheel PUT | SQLite Python PUT |
|---|---:|---:|
| batch = 1 | 3,975.87 ops/s | 9,151.63 ops/s |
| batch = 128 | 83,802.72 ops/s | 267,822.64 ops/s |

The native benchmark against SQLite, LMDB, LevelDB and RocksDB completed successfully and remains part of the evidence bundle. Claims must remain workload-specific and reproducible.

## Compatibility

Published `v0.1.0` remains the immutable historical research baseline.

The release candidate uses BDR3 snapshots and BDW3 WAL segments. Any future incompatible format change must use an explicit new version/magic and migration documentation.

## Licensing

Software remains under the **BDR Academic and Non-Commercial Research License v1.0**.

Academic, educational and non-commercial research use is permitted under the license terms. Commercial use, production deployment, proprietary integration, SaaS and monetized use require a separate written commercial license from ETBRA Tecnologias.

No patent license is granted.

## Classification

**Experimental / Release Candidate**

The project is not represented as production-ready and does not claim strict worst-case O(1) complexity for the complete database engine.

## Publication state

The v0.2.0-rc1 metadata is aligned in `CITATION.cff`, `.zenodo.json`, the Python package metadata and the repository release documentation. The software DOI recorded for this release candidate is **10.5281/zenodo.22074886**.
