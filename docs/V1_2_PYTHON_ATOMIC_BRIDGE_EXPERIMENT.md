# BDR v1.2 experiment — Python Atomic Bridge

Status: experimental implementation for Resolutive-DB Issue #25. The published BDR v1.1.0 baseline remains unchanged.

## Purpose

Expose the already validated BDR v1.1 `AtomicDatabase` / BDW4 semantics to Python without copying persistence logic into Python and without changing the on-disk format.

The bridge is intended for the post-v1 Memoria.ia topological/temporal prototype, where one logical observation may contain many physical records such as reusable addresses, composition edges, provenance, temporal events and state transitions.

## Boundary

```text
Memoria.ia Python
      |
      v
bdr.AtomicBDR
      |
      v
atomic C ABI (shared library)
      |
      v
bdr::AtomicDatabase
      |
      v
BDW4 / BDR3 / BDW3 persistence
```

BDR remains responsible only for durable key/value persistence, atomic batches, sequence boundaries and recovery. Entity semantics, temporal operators, ontology and topology remain Memoria.ia responsibilities.

## Additive Python surface

```python
from bdr import AtomicBDR, DurabilityMode, Operation

with AtomicBDR.open("./memory.bdr") as db:
    result = db.write_batch(
        [
            Operation.put("node:azul", b"..."),
            Operation.put("event:12", b"..."),
        ],
        durability=DurabilityMode.BATCH_SYNC,
    )

    value = db.get("node:azul")
    last = db.last_sequence()
    durable = db.durable_sequence()
```

Additional helpers:

- `put_many(items, durability=...)`
- `erase_many(keys, durability=...)`
- `sync()`
- `close()`

Keys accept `str` or `bytes`; strings are encoded as UTF-8. Values accept strings or arbitrary bytes-like payloads. `get()` always returns exact `bytes` or `None`.

## Native ABI compatibility

The pre-existing `bdr_atomic_c_write_batch()` function remains unchanged and retains `BatchSync` behavior.

An additive `bdr_atomic_c_write_batch_with_durability()` entry point exposes the existing native durability modes:

- `Async`
- `BatchSync`
- `PerOperationSync`

This does not change batch atomicity. It only selects the durability boundary already implemented by `AtomicDatabase`.

A shared-library form of the same atomic C ABI is built for dynamic consumers such as Python `ctypes`. The static target used by mobile/native consumers remains available.

## Validation gates for this experiment

The dedicated integration test must verify:

1. binary values containing NUL and non-UTF-8 bytes round-trip exactly;
2. UTF-8 keys and values round-trip exactly;
3. an atomic batch receives one monotonic native sequence;
4. BatchSync is durable when it returns;
5. Async remains atomic but is not reported durable until `sync()`;
6. delete and replacement survive close/reopen;
7. `last_sequence()` and `durable_sequence()` survive reopen;
8. the Memoria.ia E12/E24/E31 acceptance fixture can be reconstructed exactly after restart;
9. stable address records are unchanged after restart;
10. existing root Python tests and native v1.1 atomic restart/torn-tail tests remain green.

## Non-goals

- no SQL layer;
- no temporal query engine in BDR;
- no Memoria.ia ontology in BDR;
- no new disk framing;
- no rewrite of BDW4;
- no change to the frozen `bdr::Database` v1.0 surface;
- no stable v1.2 release until cross-platform and crash/restart gates are complete.

## Promotion rule

This bridge remains experimental until CI validates the native library, Python integration, v1.1 regressions, crash/restart behavior and the representative Memoria.ia fixture. Promotion should be additive and must retain v1.1 as a reproducible comparison baseline.
