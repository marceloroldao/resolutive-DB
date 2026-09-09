# BDR v1.2 Freeze Candidate Record

Status: **candidate for RC formation; not yet a release**

Candidate head before this record: `6d46648578bb7187c552d2cdc5faae8151ba1ef9`

This record consolidates the evidence required to decide whether the experimental v1.2 line can be promoted to a release candidate without changing the frozen BDR v1.1 storage semantics or the Memoria.ia semantic contract.

## Scope admitted for v1.2 candidate

- Python `AtomicBDR` bridge over the atomic C ABI;
- selectable durability (`Async`, `BatchSync`, `PerOperationSync`);
- atomic logical batches and sequence visibility;
- additive bulk reads (`get_many` and packed-output fast path with compatibility fallback);
- Atomic C ABI v2 for the expanded v1.2 surface;
- shared-library build for Python/FFI consumers;
- Android NDK arm64-v8a ABI validation;
- experimental diagnostics/telemetry used to profile reload/save behavior;
- benchmark evidence against the frozen Memoria.ia topological/temporal workload.

The following remain outside the stable semantic contract unless explicitly promoted in release documentation:

- benchmark-only bulk-prefetch adapters;
- diagnostic timing fields as long-term stable API guarantees;
- any Memoria.ia ontology, entity-resolution or temporal-query semantics;
- any claim of universal BDR superiority over SQLite.

## Preserved invariants

- no BDW4 framing redesign;
- BDR remains key -> bytes persistence and does not absorb Memoria.ia semantics;
- one logical snapshot/batch is atomic and sequence ordered;
- torn-tail recovery remains fail-safe;
- BDR3/BDW3 side-by-side compatibility remains intact;
- v1.0/v1.1 public compatibility surface remains preserved;
- exact binary payloads and UTF-8 keys/values remain supported;
- existing non-packed bulk-read path remains available as compatibility fallback.

## Validation head

Validated experimental head: `6d46648578bb7187c552d2cdc5faae8151ba1ef9`.

All observed PR-triggered gates on that head completed successfully:

- BDR CI;
- V101 Atomic Batch;
- V102 File WAL;
- V103 Commit Boundary;
- V104 Batch API;
- V105 Concurrency;
- V106 Migration;
- V107 Integrated Candidate;
- V121 Topological Reload Telemetry;
- V122 Frozen Memoria Topological Benchmark;
- V123 Frozen Memoria Decode Decomposition;
- V124 Get Many Buffer Decomposition;
- V125 Packed Key Marshalling;
- V126 Packed Output Probe;
- V127 Frozen Memoria Extended Scale;
- Android NDK C ABI, including arm64-v8a symbol verification and host restart/recovery coverage.

No unresolved PR #29 review threads were present at freeze review time.

## Frozen Memoria.ia oracle

Validated Memoria.ia commit:

`99a1585d497b98f0fc6f360ec8f39e6771452827`

The BDR fixture was checked against the frozen multi-size benchmark construction and preserves the same subject/attribute/value cycles, raw text generation, `xqz91` rare token, `ingest_text`, and `observe_state` calls. The BDR benchmark imports the frozen persistence codec rather than duplicating domain semantics in BDR.

## 1200-observation acceptance result

Current representative V122 result on the validated candidate:

- 7,421 physical records;
- BDR disk: 3,747,016 bytes;
- BDR cold open: ~12.40 ms;
- BDR `bulk_get`: ~11.29 ms;
- BDR semantic bulk-prefetch rebuild: ~82.40 ms;
- SQLite oracle load: ~57.36 ms;
- semantic parity: GREEN for current state, topology metrics, raw count, event count and transition count.

The original frozen comparison was approximately BDR reload ~130 ms versus SQLite ~72 ms. The BDR-side gap was therefore materially reduced without changing BDW4 framing or Memoria semantics.

Profiling shows the remaining end-to-end reload cost is dominated by semantic JSON/object reconstruction outside native WAL replay. This is evidence against redesigning BDW4 for the current bottleneck.

## Extended scale evidence

### 5,000 observations

- 30,601 physical records;
- BatchSync: ~317.74 ms;
- cold open: ~32.96 ms;
- bulk get: ~41.57 ms;
- semantic bulk-prefetch rebuild: ~313.50 ms;
- SQLite oracle load: ~380.08 ms;
- BDR disk: 15,776,424 bytes;
- SQLite disk: 27,226,112 bytes;
- semantic parity: GREEN.

### 10,000 observations

- 61,101 physical records;
- BatchSync: ~742.13 ms;
- cold open: ~67.06 ms;
- bulk get: ~85.08 ms;
- semantic bulk-prefetch rebuild: ~814.99 ms;
- SQLite oracle load: ~591.63 ms;
- BDR disk: 31,600,715 bytes;
- SQLite disk: 54,820,864 bytes;
- semantic parity: GREEN.

The 10k end-to-end result is a recorded negative performance result: the BDR-backed semantic rebuild is slower than the SQLite oracle on that runner. Native BDR reopen remains comparatively small, so no additional BDR format optimization is justified without new profiling evidence.

## ABI decision

The expanded v1.2 Atomic C ABI is designated **ABI version 2**.

Policy:

- v1.2 consumers may detect v2 features explicitly;
- Python preserves feature-detection/fallback behavior when packed bulk-read symbols are unavailable;
- ABI v1 compatibility is not silently redefined;
- further ABI changes before RC require a new full regression cycle and an update to this record.

## Freeze gate assessment

- frozen Memoria semantic parity: PASS
- restart/torn-tail atomic recovery: PASS
- BDR3/BDW3 compatibility: PASS
- Python/C++/Android alignment: PASS
- legacy regression gates: PASS
- positive and negative benchmark evidence: PASS
- 24/240/1200 exact frozen workload: PASS
- 5k/10k extension: PASS semantically, with 10k performance caveat documented
- material reduction of the 1200 reload gap: PASS
- no evidence requiring BDW4 redesign: PASS

## Conditions before `v1.2.0-rc1`

1. Keep this candidate branch free of new feature work.
2. Decide the exact package/release version bump from experimental `1.1.0` metadata to `1.2.0rc1`.
3. Align README, pyproject/package metadata, C/C++ ABI documentation and release notes with the final RC contract.
4. Re-run the complete candidate gates on the post-versioning RC commit.
5. Do not publish a stable `v1.2.0` tag until the RC cycle is validated.

Conclusion: the experimental BDR v1.2 line is **ready to enter RC formation**, not yet ready to be called a stable release.