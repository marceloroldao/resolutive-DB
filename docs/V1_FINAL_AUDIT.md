# BDR v1 — Clean Promotion Repository Audit

Status: **clean promotion candidate / technical review**. This document does not release or publish v1.

## Scope

This audit records the repository state before any future v1 promotion to `main`. Historical public-release metadata must remain historical until v1 is explicitly approved.

## Licensing — PASS

- `LICENSE` is present and internally consistent.
- Software is explicitly source-available and non-commercial, not represented as OSI-approved open source.
- Academic, educational and non-commercial research use is granted under the repository license.
- Commercial use requires a separate written commercial license.
- No patent license is granted.
- No v1 engine change currently requires a license-text change.

## Current public-release metadata — PASS / PRESERVE

The following files correctly describe the current published line `v0.2.0-rc1` and must not be rewritten to v1 before actual promotion:

- `README.md`
- `CITATION.cff`
- `.zenodo.json`
- `pyproject.toml`
- `RELEASE_NOTES_v0.2.0-rc1.md`
- `docs/release/v0.2.0-rc1-evidence.md`

This is intentional historical/public state, not stale data to be silently changed during v1 preparation.

## v1 clean-promotion governance — PASS

- promotion work is isolated on `promotion/v1-clean`, created directly from `main`.
- the experimental/internal ~99-commit history is not being bulk-merged.
- the clean branch carries only the proven v1 subset plus its validation/docs.
- no v1 release tag, DOI or public release metadata is introduced by the clean promotion work.
- the preserved baseline implementation remains available.
- no merge into `main` is authorized by this audit.

## v1 API/package state — PASS

- public C++ source API is documented in `docs/V1_PUBLIC_API.md`.
- public header is CI-locked by `.github/v1-public-api.lock`.
- v1 distribution policy is static CMake package `bdr::bdr`.
- shared-library cross-version ABI compatibility is not claimed.
- clean external package-consumer validation passes.

## Persistence and recovery state — PASS

- BDR3 snapshot format preserved.
- BDW3 WAL format preserved.
- four-direction cross-version persistence compatibility passes.
- 12 checkpoint crash-boundary failpoints pass.
- repeated checkpoint/reopen churn passes on candidate and fallback.
- clean promotion CI runs 200 checkpoint/reopen cycles on both candidate and fallback.

## Long soak — PASS

The materialized `database_v1.cpp` completed the 50M mutation campaign successfully:

- operations: 50,000,000
- cycles: 5,000
- final records: 77,749
- elapsed wall time: 24:43.33
- peak RSS: 47,552 KB
- swaps: 0
- exit status: 0

The scale gate reported PASS and GitHub Actions uploaded the soak evidence artifact.

## Resource state — PASS

Resource acceptance is evaluated comparatively on the same hosted runner to avoid treating runner-to-runner RSS variation as an engine regression.

Latest clean paired 1M-record evidence:

- candidate RSS: 279,156 KB
- fallback RSS: 347,256 KB
- candidate disk: 53,777,840 bytes
- fallback disk: 53,777,840 bytes
- records: exactly 1,000,000 in both paths
- swaps: 0 in both paths

The clean CI requires:

- exact 1,000,000-record cardinality in both paths;
- zero swap in both paths;
- candidate persistent footprint <= 55,000,000 bytes;
- identical candidate/fallback persistent footprint;
- candidate to retain at least a 5% same-runner RSS advantage over fallback.

The latest clean run retained about a 19.6% RSS advantage and passed the gate.

## Clean branch CI — PASS

The latest complete clean-promotion validation closed green for:

- public API lock;
- native CMake build and CTest contract suite;
- four-direction persistence compatibility;
- installed-package external consumer;
- checkpoint crash-boundary gate;
- paired candidate/fallback resource gate;
- 200-cycle checkpoint/reopen churn on candidate;
- 200-cycle checkpoint/reopen churn on fallback.

The 50M soak is not repeated on every push; it is a manual expensive gate and has already passed against the byte-identical materialized v1 engine source.

## Current clean diff — PASS

At the last audit, `promotion/v1-clean` was ahead of `main` by six commits and changed/added only the v1 core/package/test/documentation subset. The branch does not contain the broad v0.3 workflow/ablation history that exists in the internal experimental line.

## Promotion-only changes — HOLD

Only after explicit approval to promote v1 should the release-closing work update:

1. `README.md` current release/status section;
2. `CITATION.cff` version/date/release URL/DOI;
3. `.zenodo.json` title/description/keywords/notes as required;
4. `pyproject.toml` version and release description;
5. `CHANGELOG.md` v1 entry;
6. v1 release notes and release evidence;
7. any final tag/release metadata.

The DOI field must not be invented. It should be inserted only from the actual publication/release record.

## Technical conclusion

The clean promotion subset is technically closed for review based on the recorded gates. This audit does **not** authorize a merge, tag, release, DOI, or publication. Those remain explicit future decisions.
