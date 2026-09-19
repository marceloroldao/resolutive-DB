# BDR v1.2.0-rc2 — Release Candidate Notes

Status: release candidate under final validation. This is a new candidate and does not modify the published v1.2.0-rc1 artifact or DOI. BDR v1.1.0 remains the published stable baseline.

## Purpose

BDR v1.2.0-rc2 carries the v1.2.0-rc1 candidate surface forward and adds narrowly bounded recovery compatibility for legacy BDW4 WALs produced by historical Memoria.ia runtimes that opened two independent BDR handles on the same data directory.

## Legacy BDW4 compatibility

The historical dual-handle writer can produce a WAL whose first committed batch sequences are `1,1,2,3...`. RC1 correctly rejects the repeated sequence under its strict sequence-order rule, but that prevents cold reopening of preserved production state written by the affected legacy runtime.

RC2 admits only the exact legacy restart shape at the head of the WAL:

- initial replay sequence is zero;
- exactly one batch with sequence 1 has already committed;
- the immediately following valid batch also has sequence 1;
- subsequent batches must resume normal monotonic ordering at sequence 2 and above.

CRC, magic and version validation remain mandatory before this compatibility rule is considered.

## Rejection invariants

The compatibility rule does not generalize duplicate acceptance:

- `1,1,2` is accepted;
- `1,1,1` is rejected;
- `1,2,2` is rejected;
- ordinary sequence gaps remain rejected.

No BDW4 framing redesign is introduced.

## Production cross-version evidence

The Memoria.ia Server production update/rollback gate generated the legacy WAL using the historical runtime and exactly 300 episodes. The WAL reproduced the expected `1,1,2...` prefix.

With the bounded recovery change:

- legacy state cold-reopened successfully;
- Memoria.ia reported `UPGRADED_ROWS 300`;
- BDR Explorer read the preserved state successfully;
- rollback health returned successfully;
- rollback restored `ROLLBACK_ROWS 300`.

The production validation therefore exercised a real legacy-generated WAL rather than only a synthetic unit fixture.

## Regression evidence

The BDR pull-request gate passed the atomic WAL regression plus the broader BDR candidate workflows, including BDR CI, durability, crash recovery, concurrency, migration and integrated stress.

## Versioning

- Python package: `1.2.0rc2`.
- Intended Git tag after final release-branch validation: `v1.2.0-rc2`.
- Atomic C ABI version remains 2.
- Published `v1.2.0-rc1` remains immutable.
- This candidate is not the stable `v1.2.0` release.

## Promotion rule

No tag, GitHub release, Zenodo record, or stable promotion should be created until the final CI round on the versioned release commit is green.
