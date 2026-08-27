# Changelog

## 1.1.0 — Final / Publication Ready

Backward-compatible evolution of the stable v1 line focused on atomic logical-memory writes for Memoria.ia while preserving the immutable v1.0.0 baseline.

### Public API
- Existing `bdr::Database` v1.0 source calls remain unchanged and continue to compile through the installed `bdr::bdr` target.
- New additive `bdr::AtomicDatabase` surface is exported through `bdr/atomic_database.hpp`.
- Public atomic operations: `write_batch`, `put_many`, `erase_many`, single-key `put`/`erase`, `sync`, `last_sequence` and `durable_sequence`.
- Explicit durability modes: `Async`, `BatchSync` and `PerOperationSync`.
- `PerOperationSync` is restricted to exactly one operation; multi-operation atomic batches use `BatchSync`.

### Persistence and migration
- Existing BDR3 snapshots and BDW3 WAL segments remain readable.
- New atomic writes use explicitly versioned BDW4 frames rather than changing BDW3 interpretation.
- Side-by-side migration preserves v1.0 persistence files unchanged.
- Torn/incomplete BDW4 final frames are discarded as a unit and repaired to the last valid boundary.
- Each batch carries one monotonic commit sequence and complete-frame CRC validation.

### Correctness and durability evidence
- V101 atomic framing: PASS.
- V102 file WAL: PASS.
- V103 commit-boundary failpoints: PASS.
- V104 batch API: PASS.
- V105 concurrency: PASS.
- V106 BDR3/BDW3 migration: PASS.
- V107 integrated candidate: PASS.
- V108 integrated stress/soak: PASS.
- V109 integrated crash-recovery matrix: PASS.
- V110 durability contract: PASS.
- V111 installed public API compatibility: PASS.
- V112 Memoria.ia representative workload: PASS.
- BDR CI: PASS.
- V99 evidence closure: PASS.
- V100 evidence closure: PASS.
- Post-merge BDR CI on `main`: PASS.

### Memoria.ia acceptance evidence
Representative V112 workload: 512 logical memories × 24 physical records = 12,288 records, with one durability boundary per logical memory.

After V113 removed an unnecessary full-state copy from each recovered BDW4 batch:
- v1.0-cadence write: 4,918.203 ms;
- v1.1 atomic write: 1,696.555 ms (~2.90x faster in this run);
- v1.0 reopen + full verify: 29.120 ms;
- v1.1 atomic reopen + full verify: 13.851 ms (~2.10x faster in this run);
- v1.0-cadence disk: 3,829,120 bytes;
- v1.1 atomic disk: 3,597,664 bytes.

These figures are workload- and runner-specific and are retained as regression evidence, not universal performance claims.

### Publication policy
- The technical v1.1.0 tree is integrated and validated.
- Publication metadata is staged without inventing or predeclaring a v1.1.0 DOI.
- The definitive software DOI is added only after the publication service returns it.

## 1.0.0 — Final / Released

First stable-engine line of the Resolutive Database Engine (BDR). Software DOI: 10.5281/zenodo.22120246.

### Core
- `CompactIndex` is the preferred internal index for the v1 engine.
- `ResolutiveIndex` remains available internally as a regression fallback.
- Public C++ source API is frozen through `database.hpp` and CI lock.
- Stable CMake consumer target: `bdr::bdr`.
- Repository-root CMake entry point supports normal build/install workflows.

### Persistence and compatibility
- BDR3 snapshots preserved.
- BDW3 WAL preserved.
- Streaming checkpoint path validated.
- Four-direction persistence compatibility validated between baseline, candidate and fallback.
- Torn-WAL recovery preserved.
- Twelve deterministic checkpoint crash-boundary failpoints pass.

### Validation
- 50,000,000-operation materialized soak: PASS.
- 200 checkpoint/reopen cycles with 2,000,000 operations: PASS on candidate and fallback.
- Root-level CMake contracts: PASS.
- Root-level package install and external consumer: PASS.
- Root-level persistence compatibility: PASS.
- BDR CI: PASS.
- V99 evidence closure: PASS.
- V100 evidence closure: PASS.

## 0.2.0-rc1 — Experimental / Release Candidate

First API-capable release-candidate line of the Resolutive Database Engine (BDR).

### Core and integration
- Consolidated persistent C++ database core.
- Deterministic `rho + local Robin Hood + fingerprint` addressing path.
- Variable-length binary values.
- C++ API for open/get/put/delete/wait/sync/checkpoint/close.
- Frozen C ABI v1.
- Native Python package `bdr-native` version `0.2.0rc1`.

### Persistence
- BDW3 write-ahead log with sequence and integrity checks.
- BDR3 snapshots/checkpoints with CRC validation.
- Streaming snapshot recovery.
- Torn final WAL tail repair.
- Exclusive process-level database lock.
- Writer I/O error propagation through the API.

### Validation
- V100 evidence closure: `candidate: true`.
- V86 core contract: PASS.
- 500,000-mutation soak: PASS.
- C ABI v1 contract and exact symbol set: PASS.
- Installed Python wheel: PASS.
- Python and native market benchmarks completed.
- Readiness V95, metadata V97 and staging V98 audits: PASS.

### Publication
- Software DOI: 10.5281/zenodo.22074886.
- v0.1.0 remains the immutable historical research baseline.

### Status
Experimental release candidate; not production-ready. Performance claims remain workload-specific and the project does not claim strict worst-case O(1) complexity for the complete engine.

## 0.1.0 — Experimental / Research Preview

Initial reproducible research release of the Resolutive Database Engine (BDR).

### Core
- Deterministic resolutive addressing experiments using rho_R, phase-derived channels and signatures.
- In-memory baseline implementation and extensive benchmark history against Python dict, binary search, SQLite, flat hash maps and C++ unordered_map.
- Partitioned C++ prototypes using local Robin Hood hashing.
- Compact per-partition representation for read-heavy workloads.
- Adaptive partition experiments selecting Compact or Robin Hood according to local density.

### Persistence
- Integrated persistent engine with PUT, GET and DELETE.
- Segmented append-only WAL with monotonic sequence numbers and CRC32 per record.
- Group commit and explicit durable writes.
- Versioned BDR2 snapshots with CRC32.
- Atomic checkpoint protocol using temporary snapshot, fsync, rename and directory fsync.
- Safe WAL retirement after committed checkpoints.
- Strict recovery checks for checksum failures, unknown operations and sequence gaps.
- Legacy BDR1 snapshot read compatibility.

### Validation
- Differential fuzz testing against reference dictionaries.
- 500k-operation soak test with periodic checkpoint/reopen cycles and zero observed divergence in the recorded run.
- Multiwriter tests.
- Real POSIX SIGKILL crash-recovery test in GitHub Actions.
- Python 3.11 and Python 3.12 CI gates.
- C++ benchmark compile/smoke gates.
- SQLite WAL/FULL durability-parity benchmark with equivalent commit boundaries and final-state equality checks.

### Status
This release is a research preview, not a production database. Complexity and performance claims remain benchmark-dependent. In particular, the project does not claim strict worst-case O(1) lookup for every engine configuration.
