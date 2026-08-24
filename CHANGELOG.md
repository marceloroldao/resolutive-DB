# Changelog

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
