# V91 — API / ABI Freeze Draft

Status: **experimental, not released**

This document defines the freeze criteria for the first public API-capable BDR release candidate. No release is authorized by this document.

## C ABI v1 required symbols

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
3. Existing fields may not change order, type or meaning after freeze.
4. New optional fields may only be appended and must be guarded by `struct_size`.
5. Existing enum numeric values may not change.
6. Binary keys and values are represented by pointer + explicit length; embedded zero bytes are valid.
7. `bdr_get` remains a two-call size/query contract.
8. `bdr_put` / `bdr_delete` return durability tickets; `bdr_wait` establishes durability for that ticket.
9. `bdr_put_sync` / `bdr_delete_sync` return only after durability confirmation or explicit error.
10. I/O failure must propagate as an error; it must not terminate the host process.
11. Concurrent opens of the same database directory by separate processes are rejected while the owner holds the lock.

## Disk compatibility rules

- Candidate uses BDR3 snapshots and BDW3 WAL segments.
- Hardening changes must not silently alter BDR3/BDW3 interpretation.
- Any future incompatible format requires a new explicit version/magic and migration documentation.

## Freeze gates

Freeze is allowed only after all of the following have reproducible evidence:

- consolidated V86 contract;
- C ABI V87 contract;
- Python wheel V88 clean install;
- Python installed-package benchmark V89;
- native market benchmark V90;
- crash/SIGKILL recovery;
- multiwriter races;
- close-vs-submit race;
- checkpoint-vs-writer race;
- PUT/DELETE same-key race;
- ASan / UBSan / TSan;
- long soak;
- format compatibility test;
- process-lock test;
- writer I/O error propagation test.

If any gate is unresolved, API/ABI remains experimental.
