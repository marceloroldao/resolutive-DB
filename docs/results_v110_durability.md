# V110 — Explicit Durability Contract Evidence

Status: **PASS**

Validated on `develop/v1.1.0` at commit `9dbb6f4a2afe6a6c86f7eb04f57fa203c0690d77`.

GitHub Actions workflow: `V110 Durability Contract`, conclusion `success`.

## Contract validated

`DurabilityMode` now has the three planned modes:

- `Async`: append the complete atomic BDW4 frame without an immediate durable flush. `last_sequence` advances while `durable_sequence` remains at the previously confirmed prefix.
- `BatchSync`: append one complete atomic batch and cross one durable flush boundary before returning a durable result.
- `PerOperationSync`: durable single-operation commit; multi-operation use is rejected to avoid weakening or misrepresenting atomic batch semantics.

## Test coverage

- multi-operation async batch;
- additional async single operation;
- explicit `sync()` promotes the complete visible prefix to the durable sequence;
- multi-operation `BatchSync` commit;
- single-operation `PerOperationSync` commit;
- rejection of `PerOperationSync` for a multi-operation batch without sequence advancement;
- durable multi-key delete helper;
- reopen with exact final sequence and state reconstruction.

## Interpretation

V110 closes the explicit durability-mode requirement from the v1.1 roadmap at the experimental candidate level. The next gate is public API promotion/source compatibility: existing v1.0 calls must continue compiling and behaving as before while the atomic batch and bulk APIs become an additive v1.1 surface.
