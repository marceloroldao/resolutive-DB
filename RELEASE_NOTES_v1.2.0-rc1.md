# BDR v1.2.0-rc1 — Release Candidate Notes

Status: release candidate under final validation. Not the stable line. BDR v1.1.0 remains the published stable baseline until this candidate is explicitly promoted.

## Scope

BDR v1.2.0-rc1 promotes the validated experimental Python atomic bridge and measured bulk-read improvements developed against the frozen Memoria.ia topological/temporal workload. The release candidate preserves the v1.1 storage semantics and BDW4 framing.

## Candidate surface

- Python `bdr.AtomicBDR` bridge.
- Exact binary payloads and UTF-8 key/value support.
- `get`, `get_many`, `write_batch`, `put_many`, `erase_many`, `sync`, `last_sequence`, `durable_sequence`, and `close`.
- Durability modes `Async`, `BatchSync`, and `PerOperationSync`.
- Atomic C ABI v2.
- Packed input-key marshalling in Python `get_many`.
- Optional packed-output native fast path with one output arena.
- Runtime fallback to the legacy `get_many` symbol when packed output is unavailable.
- Android NDK arm64-v8a export validation for the candidate ABI.

## Compatibility invariants

- No BDW4 framing redesign.
- BDR3/BDW3 side-by-side compatibility remains intact.
- `bdr::Database` compatibility remains preserved.
- Existing atomic write semantics remain all-or-none.
- Torn-tail recovery remains fail-safe to the last valid commit boundary.
- Python bridge fallback preserves compatibility with native libraries that do not export `bdr_atomic_c_get_many_packed`.
- Memoria.ia ontology, temporal resolution, provenance semantics, and object reconstruction stay outside BDR.

## Frozen Memoria.ia validation

Reference Memoria.ia commit:

`99a1585d497b98f0fc6f360ec8f39e6771452827`

Validated sizes: 24, 240, 1200, 5000, and 10000 observations.

All tested sizes preserve semantic parity for current state, topology metrics, raw-memory count, event count, and transition count.

Representative 1200-observation freeze run:

- BDR bulk-prefetch semantic rebuild: ~82.4 ms.
- BDR `bulk_get`: ~11.3 ms.
- BDR cold open: ~12.4 ms.
- SQLite oracle load: ~57.4 ms.
- BDR disk: ~3.75 MB.
- SQLite oracle disk: ~6.32 MB.

The original issue baseline was approximately 130.1 ms BDR reload versus 71.6 ms SQLite. The measured gap was therefore materially reduced without changing BDW4 or Memoria semantics.

Extended-scale evidence is intentionally retained even when unfavorable:

- 5000 observations: BDR bulk rebuild ~313 ms versus SQLite oracle ~380 ms.
- 10000 observations: BDR bulk rebuild ~815 ms versus SQLite oracle ~592 ms.
- 10000-observation BDR cold open: ~67 ms.

The 10k result indicates that remaining end-to-end reload cost is dominated by semantic/object reconstruction outside the BDR native replay path. No universal performance superiority claim is made.

## Validation gates

The freeze candidate line passed:

- BDR CI;
- V101–V107;
- V121–V127;
- Android NDK C ABI arm64-v8a;
- native restart and torn-tail recovery;
- Python bridge regression;
- packed-key binary/UTF-8/missing/empty/duplicate/order tests;
- explicit fallback test without `get_many_packed`.

## Versioning

- Python package: `1.2.0rc1`.
- CMake project version: `1.2.0` with RC1 status documented separately.
- Intended Git tag after final versioned-commit validation: `v1.2.0-rc1`.
- Atomic C ABI version: 2.

No tag, GitHub release, Zenodo record, or stable promotion should be created until the final CI round for the versioned RC commit is green.

## Known non-blocking observation

At 10k observations, end-to-end semantic reload is slower than the SQLite oracle in the recorded runner. Native BDR open/replay remains a minority of that total. Further work should target Memoria-side serialization/object reconstruction only if that layer elects to optimize it; this is not justification to move cognition or semantic reconstruction into BDR.
