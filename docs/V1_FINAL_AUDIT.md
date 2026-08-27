# BDR v1 — Pre-Promotion Repository Audit

Status: **internal candidate / pre-promotion audit**. This document does not release or publish v1.

## Scope

This audit records the repository state before any future v1 promotion. Historical public-release metadata must remain historical until v1 is explicitly approved.

## Licensing — PASS

- `LICENSE` is present and internally consistent.
- Software is explicitly source-available and non-commercial, not represented as OSI-approved open source.
- Academic, educational and non-commercial research use is granted under the repository license.
- Commercial use requires a separate written commercial license.
- No patent license is granted.
- No v1 engine change currently requires a license-text change.

## Current public-release metadata — PASS / PRESERVE

The following files correctly describe the current published line `v0.2.0-rc1` and must not be rewritten to v1 before promotion:

- `README.md`
- `CITATION.cff`
- `.zenodo.json`
- `pyproject.toml`
- `RELEASE_NOTES_v0.2.0-rc1.md`
- `docs/release/v0.2.0-rc1-evidence.md`

This is intentional historical/public state, not stale data to be silently changed during internal v1 development.

## v1 internal governance — PASS

- v1 work remains on `internal/v1-candidate`.
- v1 readiness documentation explicitly says internal candidate only.
- no v1 release tag, DOI or public release metadata is introduced by the internal candidate work.
- the preserved baseline implementation remains available.

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
- 12 checkpoint crash-boundary failpoints pass on candidate and fallback.
- repeated checkpoint/reopen churn passes on candidate and fallback.

## Resource state — PASS

The materialized candidate has a fixed 1M-record CI gate requiring:

- peak RSS <= 330,000 KB;
- persistent footprint <= 55,000,000 bytes;
- exactly 1,000,000 records;
- zero swap.

The gate is based on measured candidate evidence rather than hosted-runner timing.

## Promotion-only changes — HOLD

Only after the internal v1 candidate is approved should the release-closing commit update:

1. `README.md` current release/status section;
2. `CITATION.cff` version/date/release URL/DOI;
3. `.zenodo.json` title/description/keywords/notes as required;
4. `pyproject.toml` version and release description;
5. `CHANGELOG.md` v1 entry;
6. v1 release notes and release evidence;
7. any final tag/release metadata.

The DOI field must not be invented. It should be inserted only from the actual publication/release record.

## Remaining technical hold

The materialized 50M mutation soak against `database_v1.cpp` must complete successfully before the candidate can be considered technically closed for v1 promotion.
