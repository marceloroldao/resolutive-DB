# V104 — Batch API Evidence

Status: PASS

Branch: `develop/v1.1.0`

## Objective

Validate a first consumer-facing batch API prototype on top of the V101–V103 atomic/durable WAL evidence.

## Prototype surface

- `write_batch(...)`
- `put_many(...)`
- `erase_many(...)`
- `DurabilityMode::Async`
- `DurabilityMode::BatchSync`
- `last_sequence()`
- `durable_sequence()`

## Semantics validated

- non-empty logical batches receive one sequence identity;
- `BatchSync` returns only after the durability boundary and advances `durable_sequence`;
- `Async` makes the complete batch visible without claiming durability;
- a later `BatchSync` flushes the preceding WAL prefix and advances the durable sequence across it;
- reopen reconstructs all durably covered batches;
- bulk put/delete helpers preserve batch semantics;
- empty batches are rejected explicitly.

## CI evidence

GitHub Actions workflow: `V104 Batch API`

Run #1: PASS — configure, build, `write_batch`/bulk/reopen test.

## Scope boundary

This is still an isolated prototype under `experimental/api_v104`. It does not modify the frozen BDR v1.0.0 engine. Promotion into the v1.1 candidate engine requires compatibility and concurrency validation first.
