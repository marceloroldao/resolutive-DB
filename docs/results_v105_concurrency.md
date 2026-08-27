# V105 Concurrency and Total Ordering — Evidence

Branch: `develop/v1.1.0`

## Goal

Validate that atomic multi-operation batches and single-operation helpers share one total ordering under concurrent producers, with deterministic recovery after reopen.

## Test matrix

- 8 concurrent producer threads.
- 100 atomic batches per producer.
- 800 committed batches total.
- 2 operations per concurrent batch: one unique key plus one shared-key overwrite.
- 1,600 operations executed during the concurrent phase.
- Every batch used durable `BatchSync` semantics.
- Returned commit sequences were captured and verified after all threads joined.
- Database was reopened and the complete recovered state was verified.
- Single-operation `put` and `erase` were then executed through the same ordering path and verified after a second reopen.

## Acceptance assertions

- Commit sequences are exactly `1..800` with no gaps or duplicates.
- `last_sequence == durable_sequence == 800` after the concurrent phase.
- All 800 unique keys recover with the correct values.
- The final value of the shared key is the value associated with sequence 800.
- Reopen reproduces the same state and sequence boundary.
- A simple `put` receives sequence 801.
- A simple `erase` receives sequence 802.
- A second reopen preserves sequence 802 and the erase result.
- No multi-operation batch is interleaved internally.

## Result

**PASS**

GitHub Actions workflow: `V105 Concurrency`, run `33069243532`.

This validates a single serialization point for the v1.1 candidate API. It is still an experimental gate and does not modify the frozen v1.0 engine or tag.
