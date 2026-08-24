# V91 — API / ABI Freeze Candidate

Status: **release-candidate freeze; not tagged; not published**

The V100 evidence closure completed successfully and reported `candidate: true`. This document therefore freezes the C ABI v1 contract for the staged `v0.2.0-rc1` branch. Publication still requires separate release metadata/package review.

Evidence: `docs/release/v0.2.0-rc1-evidence.md`

## C ABI v1 frozen symbols

- `bdr_abi_version`
- `bdr_options_init`
- `bdr_open`
- `bdr_close`
- `bdr_put`
- `bdr_put_sync`
- `bdr_delete`
- `bdr_delete_sync`
- `bdr_get`
- `bdr_wait`
- `bdr_sync`
- `bdr_checkpoint`
- `bdr_last_sequence`
- `bdr_durable_sequence`
- `bdr_size`
- `bdr_last_error`

## ABI rules

1. `BDR_C_ABI_VERSION == 1`.
2. `bdr_options` starts with `abi_version` and `struct_size`.
3. Frozen fields may not change order, type or meaning within ABI v1.
4. New optional fields may only be appended and must be guarded by `struct_size`.
5. Existing enum numeric values may not change.
6. Binary keys and values use pointer + explicit length; embedded zero bytes are valid.
7. `bdr_get` remains a two-call size/query contract.
8. `bdr_put` / `bdr_delete` return durability tickets; `bdr_wait` establishes durability for that ticket.
9. `bdr_put_sync` / `bdr_delete_sync` return only after durability confirmation or explicit error.
10. I/O failure propagates as an API error and must not terminate the host process.
11. Concurrent opens of the same database directory by separate processes are rejected while the owner holds the lock.

## Disk compatibility rules

- Candidate uses BDR3 snapshots and BDW3 WAL segments.
- Hardening changes may not silently alter BDR3/BDW3 interpretation.
- Any future incompatible format requires a new explicit version/magic and migration documentation.

## Evidence used for freeze

The V100 closure validated:

- consolidated V86 contract;
- C ABI V87 contract;
- exact exported ABI symbols;
- Python wheel V88 clean install;
- Python installed-package benchmark V89;
- native market benchmark V90 completion;
- 500,000-mutation soak;
- V95 readiness invariants;
- V97 release metadata audit;
- V98 staging safety audit.

Earlier experimental gates provide the crash, concurrency, process-lock, writer-error, recovery-streaming and compatibility hardening incorporated into the consolidated V86 source.

## Change policy after freeze

Any change to a frozen symbol, ABI field layout, enum numeric value, durability contract, or on-disk interpretation requires one of:

- rejection of the change for `v0.2.0-rc1`;
- a new release-candidate cycle with renewed evidence; or
- a new ABI/on-disk format version when compatibility cannot be preserved.

This freeze does not claim that BDR is faster than competing databases. Performance claims must remain workload-specific and evidence-backed.
