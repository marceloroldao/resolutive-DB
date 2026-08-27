# Changelog

## 1.0.0 — Unreleased / Finalization

First stable-engine candidate line of the Resolutive Database Engine (BDR). This section is preparatory and does not represent a published release until the v1 tag/release is explicitly created.

### Core
- `CompactIndex` is the preferred internal index for the v1 engine.
- `ResolutiveIndex` remains available internally as a regression fallback.
- Public C++ source API is frozen through `database.hpp` and CI lock.
- Stable CMake consumer target: `bdr::bdr`.
- Repository-root CMake entry point added for normal build/install workflows.
- Only `database.hpp` is installed as public API; internal index headers are not exported.

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
- Paired high-cardinality resource benchmarks show lower RSS for CompactIndex without material throughput regression.
- Root-level CMake contracts: PASS.
- Root-level package install and external consumer: PASS.
- Root-level persistence compatibility: PASS.
- BDR CI: PASS.
- V99 evidence closure: PASS.
- V100 evidence closure: PASS.

### Release policy
- No v1 DOI, tag, or release is declared by this changelog entry.
- Public metadata remains on v0.2.0-rc1 until final v1 approval and publication.
- Shared-library cross-version ABI compatibility is not promised; the supported v1 distribution contract is the static CMake target `bdr::bdr` and frozen C++ source API.

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
