# BDR v1.2.0-rc4 — Release Candidate Notes

Status: GitHub pre-release and Zenodo record published. RC4 software DOI: `10.5281/zenodo.22948288`. GitHub tag: `v1.2.0-rc4` at `317882a00f041fc1568ff986af8016b09453f21a`. BDR v1.1.0 remains the published stable baseline. Published v1.2.0-rc1, v1.2.0-rc2 and v1.2.0-rc3 artifacts remain immutable.

## Purpose

BDR v1.2.0-rc4 carries the validated v1.2 candidate surface forward and adds the durable atomic logical-clear primitive required by the current Memoria.ia native/mobile runtime.

Functional storage/ABI baseline for this candidate:

`d09914b85646353d8fd004ccf99e96a94fab9eef`

The RC4 staging commits after that SHA are release metadata, validation documentation and CI only.

## Atomic logical clear

The additive Atomic C ABI v2 symbol:

`bdr_atomic_c_clear(bdr_atomic_c_handle *, bdr_atomic_c_batch_result *)`

clears the current logical key/value state by committing one durable DELETE batch through the existing atomic WAL path. It does not truncate or replace the BDW4 WAL.

Required invariants:

- one logical clear is represented by one atomic DELETE batch;
- the returned batch result is durable;
- sequence ordering remains monotonic;
- cold reopen reconstructs the cleared logical state;
- later writes continue from the durable sequence;
- integrity checks remain green.

Atomic C ABI version remains 2 because the symbol is additive.

## Memoria.ia dependency

Current Memoria.ia calls `bdr_atomic_c_clear()` through `memoria_persistence_reset()`. The published BDR v1.2.0-rc1/rc2/rc3 headers do not expose this additive symbol, so they must not be substituted for RC4 when validating the current Memoria.ia runtime.

This candidate resolves Resolutive-DB issue #39 once the full release matrix is green and an immutable RC4 artifact is frozen/published.

## Legacy compatibility

RC4 retains the bounded legacy BDW4 recovery rules already validated by the v1.2 candidate line. It does not broaden duplicate-sequence acceptance or weaken CRC/magic/version validation.

## Versioning

- Python package: `1.2.0rc4`.
- Published Git tag: `v1.2.0-rc4`.
- Atomic C ABI version remains 2.
- Stable line remains BDR v1.1.0.
- Existing RC tags/releases remain immutable.
- This candidate is not the stable v1.2.0 release.

## Promotion rule

Publication gates passed on the versioned release commit. The tag and Zenodo record are published; no stable v1.2.0 promotion is claimed.
