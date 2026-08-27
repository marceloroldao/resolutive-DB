# V107 Integrated Candidate — Result

Status: **PASS**

GitHub Actions workflow: `V107 Integrated Candidate`
Commit: `45485457185ff1ae4fd8a8b749300ff9a55a066e`
Run: `33071866767`

## Scope

V107 consolidates the previously validated V101–V106 mechanisms behind one candidate surface:

- atomic BDW4 batch framing;
- durable append/recovery;
- batch-level sequence semantics;
- total ordering under concurrent producers;
- simple `put` / `erase` routed through the same ordering point;
- side-by-side import of v1.0 BDR3/BDW3 state;
- reopen from legacy base plus BDW4 incremental state;
- no mutation of legacy BDR3/BDW3 files.

## Integrated regression

The test creates a real v1.0 database using the v1 engine, checkpoints it, leaves additional WAL state, snapshots the legacy BDR3/BDW3 bytes, and opens the V107 candidate over that legacy directory.

It then executes 6 concurrent producers × 50 two-operation batches (300 atomic batches / 600 operations), verifies a continuous total batch sequence beginning at `legacy_sequence + 1`, routes subsequent simple `put` and `erase` operations through the same sequence, closes, reopens, and verifies the combined state and durable sequence.

Finally, the legacy BDR3/BDW3 byte image is compared before and after V107 use and remains unchanged.

## Conclusion

V107 is the first cohesive v1.1.0 engine candidate. V101–V106 remain retained as focused regression/evidence harnesses. V107 does **not** change `main`, the v1.0.0 tag, or publish a release.
