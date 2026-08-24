# BDR v0.2.0-rc1 — Draft Release Notes

**Draft only. Not released.**

This candidate is the first BDR line designed around a consumable API rather than benchmark-only prototypes.

## Highlights

- Consolidated C++ database core.
- Deterministic `rho + local Robin Hood + fingerprint` addressing path.
- C++ API for open/get/put/delete/wait/sync/checkpoint/close.
- Versioned C ABI v1 for foreign-language integration.
- Python wheel using the C ABI rather than a separate storage implementation.
- CMake install package for external C/C++ consumers.
- BDW3 write-ahead log with header and record integrity checks.
- BDR3 checkpoints with CRC validation.
- Streaming snapshot recovery to reduce peak recovery memory.
- Torn-tail repair for incomplete final WAL frames.
- Process-level exclusive database open lock.
- Writer I/O errors propagated through the API instead of terminating the host process.
- Multiwriter, checkpoint race, close/submit race, crash, soak and compatibility test gates.
- Native comparison framework against SQLite, LMDB, LevelDB and RocksDB.

## Compatibility

The published `v0.1.0` remains the immutable research baseline. This candidate does not rewrite historical benchmark results or DOI metadata.

The API candidate uses BDR3 snapshots and BDW3 WAL segments. Format compatibility must be demonstrated by the validation bundle before release.

## Classification

**Experimental / Release Candidate**

This candidate is not represented as production-ready and does not claim strict worst-case O(1) complexity for the complete database engine.

## Licensing

Software remains under the **BDR Academic and Non-Commercial Research License v1.0**. Commercial use, production deployment, proprietary integration, SaaS and monetized use require a separate written commercial license from ETBRA Tecnologias.

No patent license is granted.

## Release gate

These notes must not be promoted to official release notes unless V96 reports `candidate=true` and the release metadata audit is clean.
