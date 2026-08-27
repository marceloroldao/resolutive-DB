# BDR v1 — Selective Promotion Plan

Status: clean promotion candidate technically closed for review. This plan does not authorize merge, release or publication.

## Why selective promotion is required

`internal/v1-candidate` contains the full research path that produced the candidate: v0.3 experiments, ablations, benchmark workflows, temporary integration artifacts and v1 production-candidate files.

A direct merge of the entire internal branch into `main` would mix the final v1 surface with unnecessary experimental history and CI noise. The selective promotion branch `promotion/v1-clean` was therefore created directly from `main` and carries only the proven subset.

## Promotion state

The clean branch has already completed the technical preparation steps:

1. created directly from the public `main` baseline;
2. copied only byte-identical validated v1 implementation/test artifacts;
3. added a focused clean-promotion CI;
4. passed API lock, CMake/CTest, four-direction persistence compatibility, installed-package consumer, 12 crash failpoints, paired resource validation and candidate/fallback 200-cycle checkpoint churn;
5. preserved public v0.2.0-rc1 release metadata unchanged;
6. retained the materialized 50M soak evidence already closed on the same validated `database_v1.cpp` implementation.

## Candidate production allowlist

The proven implementation/package artifacts are:

- `experimental/api_v86/src/database_v1.cpp`
- `experimental/api_v86/include/bdr/database.hpp` (existing public header, unchanged and API-locked)
- `experimental/api_v86/include/bdr/compact_index.hpp`
- `experimental/api_v86/CMakeLists.txt`
- `experimental/api_v86/cmake/bdrConfig.cmake`
- `experimental/api_v86/package_consumer/`

The current paths retain research-era naming. Moving them into a new root-level stable layout is not required for v1 and should not be done merely for cosmetics. Any future path normalization must be separately revalidated.

## Validation allowlist

The clean branch retains the tests that directly protect v1 invariants:

- public API lock;
- candidate/fallback contract tests;
- cross-version BDR3/BDW3 compatibility;
- 12 checkpoint crash-boundary failpoints;
- 200-cycle checkpoint/reopen churn on candidate and fallback;
- clean installed-package consumer;
- same-runner paired candidate/fallback resource gate;
- materialized 50M soak gate (manual expensive gate).

The paired resource gate requires exact cardinality, zero swap, identical persistent footprint and at least a 5% RSS advantage for the compact candidate over fallback on the same runner. Hosted-runner timing is not used as a hard acceptance threshold.

## Documentation allowlist

- `docs/V1_INTERNAL_READINESS.md`
- `docs/V1_PUBLIC_API.md`
- `docs/V1_FINAL_AUDIT.md`
- `docs/V1_PROMOTION_PLAN.md`
- existing disk-format documentation relevant to BDR3/BDW3 compatibility

## Deliberately excluded from clean promotion

The following classes are not included merely because they exist in the research history:

- v0.3 ablation workflows;
- one-off market/cardinality benchmark workflows;
- obsolete shims/generators;
- intermediate experimental source wrappers;
- research result notes not required by the stable package;
- premature rewrites of public release metadata.

## Final approval sequence

No action below is authorized by this document. After explicit approval to promote v1:

1. perform one final `main` versus `promotion/v1-clean` diff review;
2. merge the approved clean subset into `main`;
3. run the post-merge stable gate suite on the exact merged SHA;
4. only then prepare/update v1 public metadata (`README`, `CITATION.cff`, `.zenodo.json`, `pyproject.toml`, `CHANGELOG`, v1 release notes/evidence);
5. create a v1 tag/release only after metadata and post-merge gates are final;
6. add an actual DOI only from the real publication record — never invent one.

## Promotion invariant

The v1 code line is the **smallest proven stable subset**, not the entire research branch that discovered it.
