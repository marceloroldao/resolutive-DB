# BDR v1.1.0 Release Checklist

Status: publication finalization / tag pending.

## Baseline protection

- [x] Published `v1.0.0` tag remains immutable.
- [x] v1.1 development was isolated in `develop/v1.1.0` / PR #11.
- [x] PR #11 remained draft during internal staging and was promoted only after required gates passed.
- [x] Validated v1.1 candidate was intentionally merged into `main`.

## Public compatibility

- [x] Existing `bdr::Database` v1.0 source calls compile unchanged.
- [x] Installed target remains `bdr::bdr`.
- [x] New atomic surface is additive via `bdr/atomic_database.hpp`.
- [x] Clean-prefix install and external `find_package(bdr)` consumer pass (V111).

## Persistence and durability

- [x] BDR3 snapshot reader preserved.
- [x] BDW3 WAL reader preserved.
- [x] New atomic format is explicitly versioned as BDW4.
- [x] Side-by-side migration does not rewrite legacy files.
- [x] Complete-frame CRC validation passes.
- [x] Torn final batch is discarded/repaired to last valid boundary.
- [x] Batch sequence is monotonic.
- [x] `Async`, `BatchSync`, and single-operation `PerOperationSync` semantics are tested.
- [x] Integrated crash matrix passes after replay optimization (V109).

## Stress and concurrency

- [x] Atomic framing regression passes (V101).
- [x] File WAL regression passes (V102).
- [x] Commit-boundary failpoints pass (V103).
- [x] Batch API regression passes (V104).
- [x] Concurrent ordering regression passes (V105).
- [x] Migration regression passes (V106).
- [x] Integrated candidate passes (V107).
- [x] Integrated stress/soak passes (V108).

## Memoria.ia acceptance

- [x] Representative logical-memory batch contains multiple physical records.
- [x] Logical batch is all-or-none after crash.
- [x] Representative write performance does not regress versus v1.0 cadence.
- [x] Reopen/full verification does not regress after V113 optimization.
- [x] Representative disk footprint does not regress.
- [x] V112 benchmark evidence is retained in repository documentation.

## Release metadata and documentation

- [x] Root CMake project version is `1.1.0`.
- [x] Root Python package and `bdr.__version__` are staged as `1.1.0`.
- [x] `CHANGELOG.md` contains final/publication-ready v1.1 entry.
- [x] `docs/V1_1_PUBLIC_API.md` documents compatibility and durability semantics.
- [x] `RELEASE_NOTES_v1.1.0.md` finalized for publication staging.
- [x] `README.md`, `CITATION.cff`, and `.zenodo.json` staged for v1.1.0.
- [x] No invented/predeclared v1.1.0 DOI.

## Final promotion gates

- [x] Final development PR diff/audit completed with no accidental unrelated changes.
- [x] Development PR head had required CI gates green after final documentation changes.
- [x] PR #11 intentionally moved out of draft only when ready for promotion.
- [x] Merge into `main` performed after explicit release decision.
- [x] Post-merge BDR CI on `main` passed (Python 3.11/3.12, C++ compile/smoke, SQLite parity).
- [ ] Final publication-metadata PR passes BDR CI, V99 and V100 with v1.1-aware audits.
- [ ] Final publication-metadata PR merged into `main`.
- [ ] `v1.1.0` tag and GitHub Release created from the final validated `main` commit.
- [ ] Definitive v1.1.0 software DOI recorded only after the publication service returns it.

Until the remaining publication gates are checked, v1.1.0 is publication-ready but not yet tagged/published.
