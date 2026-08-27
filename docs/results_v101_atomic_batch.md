# V101 — Atomic Batch Framing Evidence

Status: PASS
Branch: `develop/v1.1.0`
Workflow: `V101 Atomic Batch`

## Objective

Validate the core all-or-nothing recovery property required by Memoria.ia before integrating atomic batches into the stable BDR engine path.

## Prototype

V101 introduces an isolated candidate `BDW4` batch frame with:

- explicit total frame length;
- `BDW4` magic and version 4;
- one batch sequence;
- operation count;
- encoded put/delete operations;
- CRC32 over the complete frame contents;
- validation-before-visibility replay.

The v1.0 engine and frozen `BDR3`/`BDW3` implementation are not modified.

## Tests

The test models a Memoria.ia logical memory as multiple physical keys (payload, nodes, occurrence and metadata) and verifies:

1. every strict truncation of the first frame recovers with zero batch operations visible;
2. a complete frame makes all logical-memory records visible together;
3. a complete first batch followed by every strict truncation of a second batch preserves only the first batch;
4. complete sequential batches replay in deterministic sequence order;
5. CRC corruption fails closed without exposing partial state;
6. sequence gaps fail deterministically.

## Result

GitHub Actions run `V101 Atomic Batch` completed successfully: configure PASS, build PASS, test PASS.

## Interpretation

V101 demonstrates the framing/replay invariant needed for an atomic `write_batch`: a logical batch can be validated in full before mutating recovered state, and an incomplete final frame can be discarded as a unit.

This is not yet the v1.1 engine implementation. The next gate must validate real file append, durability boundaries and deterministic crash/failpoint recovery before integrating the primitive into `Database`.
