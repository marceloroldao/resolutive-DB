# Memoria.ia topological persistence bridge (experimental)

Status: experimental; does not alter the BDR v1.1 persistence contract.

## Purpose

Memoria.ia is validating a topological, addressable and temporal memory model. SQLite is currently used only as a restart/persistence oracle. This bridge exists so the same fixture can be persisted through BDR without introducing a second storage architecture.

## Existing BDR primitives reused

The bridge reuses, unchanged:

- `bdr::AtomicDatabase`;
- BDW4 logical atomic batches;
- `bdr_atomic_c_*` C ABI;
- arbitrary byte values and explicit key lengths;
- `last_sequence()` and `durable_sequence()`;
- sync and integrity checks;
- torn-tail recovery to the last complete batch boundary.

No Memoria.ia semantics are moved into BDR. Entity resolution, topology, temporal operators and provenance policy remain owned by Memoria.ia.

## New experimental capability

`bdr_atomic_c_api_shared` builds the existing atomic C ABI as a host shared library (`.so`, `.dylib` or `.dll`, platform dependent). It is intended for thin language adapters such as Python `ctypes`/`cffi`.

The static Android/C++ target remains unchanged and continues to exist as `bdr_atomic_c_api`.

Windows symbol export is enabled only when building/consuming the shared ABI by the `BDR_ATOMIC_C_API_SHARED` contract. Static consumers do not opt into DLL import/export annotations.

## Memoria.ia persistence mapping

A single logical observation should be one BDR atomic batch containing any new or updated physical records required by that observation, for example:

- `node/<address>` — canonical node record;
- `component/<parent>/<ordinal>` — ordered composition edge;
- `edge/<from>/<relation>/<to>` — topology edge;
- `raw/<raw-address>` — exact raw provenance bytes;
- `event/<sequence>` — append-only temporal event;
- `transition/<sequence>` — append-only state transition;
- `meta/next_sequence` — sequence allocator state if Memoria.ia chooses to persist it explicitly.

These key names are a Memoria.ia bridge convention, not a BDR storage schema.

## Required comparison

The same deterministic fixture must be stored through the SQLite oracle and BDR bridge and compared after close/reopen for:

- stable address equality;
- exact raw reconstruction;
- current state;
- previous state;
- full history;
- transitions;
- monotonic next sequence;
- event/provenance equality;
- disk growth;
- write/reopen/read latency.

No tight wall-clock pass/fail threshold should be used in CI. Performance numbers are evidence, not correctness criteria.

## Acceptance fixture

Events:

- E12 `minha_camisa.cor = azul`
- E24 `minha_camisa.cor = preta`
- E31 `minha_camisa.cor = branca`

After reopen:

- `CURRENT` → `branca`, sequence 31;
- `PREVIOUS_STATE` → `preta`, sequence 24;
- `EXISTED_IN_HISTORY(azul)` → true, sequence 12;
- `STATE_BEFORE_VALUE(preta)` → `azul`, sequence 12;
- `STATE_DIFF` → `azul → preta → branca`;
- next appended event sequence must be greater than 31.

## Freeze rule

Do not merge this experiment into a stable BDR release until shared-host build, restart, torn-tail recovery and Memoria.ia parity tests are green on the supported CI matrix.
