# Resolutive-DB v1 — Public C++ API Freeze Candidate

Status: internal v1 candidate. This document does not promote or release v1.

## Public header

The public C++ surface for the current v1 candidate is defined by:

`experimental/api_v86/include/bdr/database.hpp`

The implementation backend may change between the compact candidate and the fallback backend, but the public header must remain source-compatible.

## Frozen public types

- `bdr::Options`
- `bdr::Ticket`
- `bdr::OperationType`
- `bdr::Operation`
- `bdr::IndexStats`
- `bdr::Database`

## Frozen Database surface

- `Database::open(...)`
- destructor
- deleted copy constructor / copy assignment
- `submit(...)`
- `put(...)`
- `erase(...)`
- `put_sync(...)`
- `erase_sync(...)`
- `get(...)`
- `contains(...)`
- `wait(...)`
- `sync()`
- `checkpoint()`
- `close()`
- `last_sequence()`
- `durable_sequence()`
- `size()`
- `index_stats()`

## Compatibility rule

Before v1 is promoted, the header above is treated as frozen. Any change to names, signatures, return types, enum values, struct fields, field types, default values, namespace, or ownership semantics is a breaking API review event and must intentionally update the API lock.

Implementation-only changes that preserve this header are allowed and must continue to pass the candidate/fallback contract, crash-boundary, cross-version persistence, lifecycle, checkpoint-churn, package-consumer, and resource gates.

## v1 binary packaging policy

For v1, the supported C++ distribution model is the installed **static** CMake target `bdr::bdr`.

The v1 compatibility promise is source/API compatibility for the frozen public header plus BDR3/BDW3 persistence compatibility. v1 does **not** claim a stable cross-version binary ABI for independently compiled shared libraries (`.so`, `.dylib`, or `.dll`).

This is intentional: a shared-library ABI would require a separate symbol-visibility, SONAME/versioning, compiler/runtime compatibility and binary compatibility policy. That work is deferred beyond the first stable release rather than being implied without sufficient evidence.

Applications should link against the installed `bdr::bdr` target and rebuild when upgrading the library implementation.

## Persistence independence

The public API freeze is independent of the BDR3/BDW3 disk-format compatibility contract. Cross-version persistence is validated separately and must remain green.
