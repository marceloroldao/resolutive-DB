# V109 — Integrated Crash / Recovery Evidence

Status: **PASS**

Validated on `develop/v1.1.0` at commit `d5a06513ab80f687931cdc24dce93f22327f8fed`.

GitHub Actions workflow: `V109 Integrated Crash Recovery`, run #2, conclusion `success`.

## Coverage

The harness uses the frozen v1.0 engine to create a real legacy database containing BDR3 and BDW3 persistence, then opens it through the integrated v1.1 candidate and creates a durable BDW4 prefix.

A second logical batch contains multiple puts plus a delete. For every incomplete byte-prefix of that next BDW4 frame, the harness simulates a torn/crashed write and reopens the integrated candidate.

For every truncation point the following invariants hold:

- the previously durable BDW4 prefix remains visible;
- the incomplete new logical batch contributes zero visible operations;
- the delete from the incomplete batch is not applied;
- the torn BDW4 tail is repaired exactly back to the prior committed prefix;
- `last_sequence` and `durable_sequence` remain at the prior committed sequence;
- legacy BDR3/BDW3 files remain byte-for-byte unchanged.

The harness also validates:

- a structurally complete frame applies the whole logical batch atomically;
- the complete batch survives five repeated reopens;
- corruption inside a complete frame is rejected rather than silently replayed;
- legacy v1.0 persistence remains immutable throughout all cases.

## Interpretation

V109 closes the integrated crash/recovery gap left after V103 component-level failpoints. Together with V101–V108, it provides evidence that the v1.1 candidate preserves all-or-nothing logical batch semantics across legacy migration, durable BDW4 append, concurrent ordering, repeated reopen, torn-tail recovery, and corruption rejection.
