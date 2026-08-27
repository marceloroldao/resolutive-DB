# BDR v1 — Selective Promotion Plan

Status: internal planning only. Do not execute promotion until the materialized 50M soak and final internal gates are green.

## Why selective promotion is required

`internal/v1-candidate` contains the full research path that produced the candidate: v0.3 experiments, ablations, benchmark workflows, temporary integration artifacts and v1 production-candidate files.

A direct merge of the entire branch into `main` would mix the final v1 surface with unnecessary experimental history and CI noise. The v1 promotion should therefore be built from `main` using an explicit allowlist of proven artifacts.

## Promotion method

When the internal candidate is technically closed:

1. create a dedicated v1 promotion/integration branch from the then-current `main`;
2. copy/cherry-pick only the approved production and validation artifacts from the frozen internal candidate commit;
3. run the complete v1 gate suite on the promotion branch;
4. compare disk-format compatibility against the frozen candidate and current public baseline;
5. update public release metadata only after the promotion branch is green;
6. merge the promotion branch to `main` as the v1 code line;
7. create tag/release/DOI only after the merged commit is final.

## Candidate production allowlist

The exact final paths may be normalized during promotion, but the proven implementation artifacts are:

- `experimental/api_v86/src/database_v1.cpp`
- `experimental/api_v86/include/bdr/database.hpp`
- `experimental/api_v86/include/bdr/compact_index.hpp`
- `experimental/api_v86/CMakeLists.txt`
- `experimental/api_v86/cmake/bdrConfig.cmake`

The current locations are research-era paths. The promotion branch may move them into a stable root-level source/include layout, but only if byte/behavior equivalence is revalidated by the full gate suite.

## Candidate validation allowlist

Retain or migrate the tests that directly protect v1 invariants:

- public API lock;
- candidate/fallback contract tests;
- cross-version BDR3/BDW3 compatibility;
- 12 checkpoint crash-boundary failpoints;
- 200-cycle checkpoint/reopen churn;
- clean installed-package consumer;
- 1M fixed resource threshold gate;
- materialized 50M soak gate.

## Documentation allowlist

- `docs/V1_INTERNAL_READINESS.md` (internal evidence; may be archived after release)
- `docs/V1_PUBLIC_API.md`
- `docs/V1_FINAL_AUDIT.md`
- `docs/V1_PROMOTION_PLAN.md`
- disk-format documentation relevant to BDR3/BDW3 compatibility

## Do not automatically promote

The following classes should not be brought into v1 production merely because they exist on the candidate branch:

- v0.3 ablation workflows;
- one-off market/cardinality benchmark workflows;
- obsolete shims/generators;
- intermediate experimental source wrappers;
- research result notes not required by the stable package;
- historical release metadata rewritten prematurely.

Experimental evidence can remain in history or be retained under `experimental/`, but it must not define the stable runtime/package path.

## Promotion invariant

The v1 release is the **smallest proven stable subset**, not the entire research branch that discovered it.
