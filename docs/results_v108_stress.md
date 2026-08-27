# V108 — Integrated Stress / Soak Evidence

Status: **PASS**

Validated on `develop/v1.1.0` at commit `567151503aaa3bede1083eaab908d4f707ea05f4`.

GitHub Actions workflow: `V108 Integrated Stress`, run #2, conclusion `success`.

## Coverage

- 10 sequential open/write/reopen cycles.
- 120 atomic batches per cycle: 1,200 sequential batches total.
- Variable batch sizes (1–8 puts, with periodic deletes).
- Exact `last_sequence` and `durable_sequence` validation after every batch.
- Full state validation after every cycle reopen.
- 8 concurrent producers × 100 atomic batches: 800 concurrent batches.
- Variable concurrent batch sizes (1–6 puts).
- Exact final sequence after concurrent extension.
- 25 additional reopen-only cycles with full sequence, size, and state verification.
- No replay drift observed.

Total workload in the V108 harness: 2,000 atomic batches plus repeated full-state recovery validation.

## Regression context

On the same commit, BDR CI and workflows V101–V107, V99 and V100 also completed successfully.

## Interpretation

V108 provides sustained integrated evidence for the v1.1 candidate under repeated durable writes, variable batch sizes, concurrent producers, and repeated recovery. It does not replace crash-injection testing; integrated crash/recovery remains the next gate (V109).
