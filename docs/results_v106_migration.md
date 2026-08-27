# V106 v1.0 → v1.1 Side-by-Side Migration — Evidence

Branch: `develop/v1.1.0`

## Goal

Prove that the v1.1 candidate can consume persistence created by the frozen v1.0 engine without rewriting the v1.0 BDR3/BDW3 files, then continue the logical sequence in a separate BDW4 WAL.

## Method

The test links the actual v1 engine implementation from `experimental/api_v86/src/database_v1.cpp` to generate a real legacy database:

1. write `alpha=A1` and `beta=B1`;
2. checkpoint to BDR3;
3. update `beta=B2`, erase `alpha`, and add `gamma=G1` in BDW3;
4. capture the durable v1 sequence;
5. capture all `.bdr3` and `.bdw3` files byte-for-byte;
6. open the legacy directory through the V106 reader;
7. validate recovered state and sequence;
8. append one atomic v1.1 batch to a separate BDW4 sidecar;
9. reopen the combined v1 + BDW4 state;
10. compare all legacy BDR3/BDW3 bytes before and after migration/reopen.

## Acceptance assertions

- Real v1 BDR3 snapshot is detected and decoded.
- Real v1 BDW3 WAL is detected and replayed.
- Legacy durable sequence is preserved.
- Legacy state resolves to `beta=B2`, `gamma=G1`, with `alpha` absent.
- First BDW4 atomic batch receives exactly `legacy_sequence + 1`.
- The BDW4 batch atomically adds `delta=D1`, updates `beta=B3`, and deletes `gamma`.
- Reopen reconstructs the combined state correctly.
- Every legacy `.bdr3` and `.bdw3` file remains byte-for-byte identical before and after v1.1 writes and reopen.

## Result

**PASS**

GitHub Actions workflow: `V106 Migration`, run `33069530640`.

## Migration rule validated

The candidate migration path is additive and non-destructive:

`BDR3 + BDW3 (v1.0, immutable base) -> BDW4 (v1.1 incremental sidecar)`

This provides a reversible compatibility strategy while the v1.1 persistence contract is still experimental.
