# BDR v1.x Roadmap

Status: active post-v1 development plan
Baseline: immutable `v1.0.0`
Development branch: `develop/v1.1.0`
Primary integration driver: Memoria.ia

## Versioning policy

The `v1.0.0` tag remains immutable. Backward-compatible capability additions advance the minor line (`v1.1.0`, `v1.2.0`, ...). Patch releases (`v1.x.y`) are reserved for compatible fixes, hardening and narrowly scoped performance improvements that do not add a new public contract.

The v1 compatibility promise remains source/API compatibility for existing v1 calls and readable migration from v1.0 persistence. Any new on-disk format must be explicitly versioned and documented; existing BDR3/BDW3 data must remain recoverable by the new line.

## v1.1.0 — Atomic write path and durability contract

Goal: satisfy the first direct Memoria.ia integration requirements without changing v1.0 semantics for existing callers.

Planned capabilities:

- atomic `write_batch` transaction-like commit boundary;
- all-or-nothing crash recovery for one logical batch;
- native bulk `put_many` / `erase_many` convenience path built on the batch primitive;
- one durable batch sequence / commit identity;
- explicit durability mode contract: asynchronous, batch-sync and per-operation sync;
- compatibility reader for existing BDR3 snapshots and BDW3 WAL segments;
- deterministic crash/failpoint tests covering batch framing, partial writes and recovery;
- Memoria.ia integration benchmark and full regression gate.

Exit criteria:

- a logical Memoria.ia memory (payload + nodes + occurrences + metadata) cannot recover partially after an acknowledged atomic batch commit;
- crash before commit exposes none of the batch; crash after durable commit exposes all of it;
- existing v1.0 API calls retain their behavior;
- v1.0 persistence opens successfully in v1.1;
- new-format migration/versioning is explicit if required;
- no foreground full-checkpoint dependency is introduced.

## v1.2.0 — Platform portability and installable native boundary

Goal: make the stable engine buildable and consumable on Linux, Windows and macOS without experimental source-path coupling.

Required work:

- introduce a platform I/O abstraction for locking, positioned reads/writes, durable flush, truncate and preallocation;
- retain Linux `fallocate(..., FALLOC_FL_KEEP_SIZE, ...)` as an optimized implementation only;
- provide portable fallback behavior when keep-size preallocation is unavailable;
- replace unconditional POSIX/Linux assumptions (`flock`, `pread/pwrite`, `fdatasync`, `ftruncate`, `<linux/falloc.h>`) behind platform implementations;
- validate CMake install/consumer on supported CI hosts;
- evaluate and freeze a stable C ABI for language bindings if evidence is sufficient.

Exit criteria:

- clean compile and test on Ubuntu, Windows and macOS;
- identical logical persistence/recovery behavior across platforms;
- no platform-specific API exposed to consumers.

## v1.3.0 — Operational telemetry and workload optimization

Goal: expose enough low-cost observability for Memoria.ia and other consumers to make storage-policy decisions without inspecting private internals.

Planned capabilities:

- checkpoint telemetry: WAL bytes, dirty mutations, last checkpoint sequence/time where meaningful;
- memory/index telemetry: records, slots plus estimated/resident index memory where measurable;
- delete-heavy workload profiling and optimization;
- checkpoint scheduling guidance and asynchronous-policy examples;
- regression thresholds for read/write/delete/reopen/disk-footprint workloads.

Exit criteria:

- telemetry does not materially perturb foreground performance;
- delete-heavy regression is characterized and improved without compromising recovery;
- Memoria.ia can select checkpoint policy using public telemetry only.

## Patch-line policy

Examples:

- `v1.1.1`: crash-recovery fix or batch performance hardening preserving the v1.1 API;
- `v1.2.1`: platform build fix preserving v1.2 contracts;
- `v1.3.1`: telemetry correctness or delete-performance fix preserving contracts.

## Evidence source

The initial requirements are tracked in GitHub Issue #9, including direct Memoria.ia benchmarks and the follow-up requirement for atomic logical-memory batches.
