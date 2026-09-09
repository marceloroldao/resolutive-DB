# BDR v1.2 experiment — Python Atomic Bridge

Status: freeze-readiness candidate for Resolutive-DB Issues #25 and #28. The published BDR v1.1.0 baseline remains unchanged.

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

## Python surface under freeze review

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
    values = db.get_many(["node:azul", "event:12", "missing"])
    last = db.last_sequence()
    durable = db.durable_sequence()
```

Additional helpers:

- `put_many(items, durability=...)`
- `erase_many(keys, durability=...)`
- `get_many(keys)`
- `sync()`
- `close()`

Keys accept `str` or `bytes`; strings are encoded as UTF-8. Values accept strings or arbitrary bytes-like payloads. `get()` returns exact `bytes` or `None`. `get_many()` preserves input order and returns one `bytes | None` result per key; missing and empty values remain distinct.

## Atomic C ABI policy

The v1.2 candidate declares **Atomic C ABI version 2**.

ABI v2 is additive relative to ABI v1. All original v1 entry points remain present and keep their existing behavior. ABI v2 adds:

- `bdr_atomic_c_write_batch_with_durability()`;
- `bdr_atomic_c_get_many()`;
- `bdr_atomic_c_get_many_packed()`;
- sequence and diagnostics accessors used by the Python bridge and validation tooling.

The Python wrapper deliberately feature-detects additive symbols. A library without `bdr_atomic_c_get_many_packed()` falls back to `bdr_atomic_c_get_many()`, and a library without bulk read support falls back to repeated `get()` calls. This preserves compatibility with older native libraries while allowing the v1.2 fast path when available.

`bdr_atomic_c_get_many_packed()` returns all found values in one caller-freed arena plus per-key offsets, sizes and found flags. It is an optimization only: it does not change key/value semantics, ordering, durability or persistence format.

## Stable vs experimental surface

Freeze candidate for stable v1.2 contract:

- Python `AtomicBDR.open/get/get_many/write_batch/put_many/erase_many/sync/close`;
- `last_sequence()` and `durable_sequence()`;
- durability modes `Async`, `BatchSync`, `PerOperationSync`;
- Atomic C ABI v2 batch, get, bulk-get and packed bulk-get entry points;
- fallback compatibility with ABI v1 libraries;
- exact binary/UTF-8 behavior;
- unchanged BDW4/BDR3/BDW3 persistence compatibility.

Remains explicitly experimental unless separately promoted:

- `AtomicDiagnostics` and `bdr_atomic_c_diagnostics_get()` telemetry fields;
- benchmark-only adapters and timing decomposition;
- V121–V126 measurement scripts and runner-specific performance numbers.

Diagnostics are not a persistence-format contract and should not be used by application semantics.

## Validation evidence

Validated on the experimental line before freeze review:

1. binary values containing NUL and non-UTF-8 bytes round-trip exactly;
2. UTF-8 keys and values round-trip exactly;
3. atomic batches receive monotonic native sequences;
4. BatchSync is durable when it returns;
5. Async remains atomic but is not reported durable until `sync()`;
6. delete and replacement survive close/reopen;
7. `last_sequence()` and `durable_sequence()` survive reopen;
8. the Memoria.ia E12/E24/E31 acceptance fixture reconstructs exactly after restart;
9. stable address records are unchanged after restart;
10. bulk reads preserve order, missing values, empty values, duplicates and binary keys;
11. packed-output and legacy bulk-read paths have byte-for-byte parity;
12. fallback without the packed symbol is explicitly tested;
13. BDR CI, V101–V107 and V121–V126 have passed on the pre-freeze line;
14. the frozen Memoria.ia workload at 24/240/1200 observations preserves semantic parity;
15. Android/NDK freeze gating is required on the final candidate head.

Representative runner evidence at 1200 observations showed the public bulk-read path reduced from roughly 19–21 ms before packed output to roughly 12 ms after the packed fast path. These figures are workload- and runner-specific and are not universal performance claims.

## Non-goals

- no SQL layer;
- no temporal query engine in BDR;
- no Memoria.ia ontology in BDR;
- no new disk framing;
- no rewrite of BDW4;
- no change to the frozen `bdr::Database` v1.0 surface;
- no promotion of benchmark telemetry into application semantics;
- no stable v1.2 release until freeze gates are complete.

## Promotion rule

The published package metadata remains at `1.1.0` while this branch is experimental. A package-version bump is reserved for a release-candidate branch after all freeze gates are green.

Promotion to a v1.2 RC requires, at minimum:

- Atomic C ABI v2 gate green on host and Android arm64-v8a;
- restart and torn-tail recovery green;
- BDR3/BDW3/BDW4 compatibility green;
- Python fallback compatibility green;
- frozen Memoria.ia parity green;
- stable-vs-experimental API boundary documented;
- release checklist and notes prepared without changing the v1.1 baseline.
