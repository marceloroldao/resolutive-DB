# BDR v1.1.0 Release Checklist

Status: internal RC staging checklist.

## Baseline protection

- [x] Published `v1.0.0` tag remains immutable.
- [x] `main` remains untouched by v1.1 development.
- [x] Development is isolated in `develop/v1.1.0` / PR #11.
- [x] PR #11 remains draft during staging.

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

- [x] Root CMake project version staged as `1.1.0` on development branch.
- [x] `CHANGELOG.md` contains unreleased v1.1 entry.
- [x] `docs/V1_1_PUBLIC_API.md` documents compatibility and durability semantics.
- [x] `RELEASE_NOTES_v1.1.0.md` staged.
- [x] No invented/predeclared DOI.
- [x] No v1.1 tag or GitHub Release created during internal staging.

## Final promotion gates

- [ ] Final PR diff/audit completed with no accidental unrelated changes.
- [ ] Latest PR head has all required CI gates green after final documentation changes.
- [ ] PR #11 intentionally moved out of draft only when ready for promotion.
- [ ] Merge into `main` performed only after explicit release decision.
- [ ] `v1.1.0` tag and GitHub Release created only after merge/audit.
- [ ] Publication metadata/DOI updated only with the identifier actually returned by the publication service.

Until every final-promotion item is checked, v1.1.0 remains an internal release candidate and v1.0.0 remains the released stable baseline.
