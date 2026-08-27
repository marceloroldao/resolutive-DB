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

Implementation-only changes that preserve this header are allowed and must continue to pass the candidate/fallback contract, crash-boundary, cross-version persistence, lifecycle, and checkpoint-churn gates.

## ABI note

This is primarily a C++ source/API freeze. A stable binary ABI across independently compiled shared-library versions is not claimed yet. The current build uses static-library targets. A binary ABI guarantee, shared-library versioning policy, install/export package layout, and symbol visibility policy must be defined separately before claiming ABI stability.

## Persistence independence

The public API freeze is independent of the BDR3/BDW3 disk-format compatibility contract. Cross-version persistence is validated separately and must remain green.
