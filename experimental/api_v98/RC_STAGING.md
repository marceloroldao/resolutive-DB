# BDR v0.2.0-rc1 — Release Candidate Staging

Status: **NOT RELEASED / NOT TAGGED / NOT PUBLISHED**

This staging document prepares the first BDR release candidate that includes a consumable API. It must not be used as evidence that v0.2.0-rc1 has been released.

## Candidate identity

- Proposed version: `0.2.0-rc1`
- C ABI: `1`
- Persistent snapshot format: `BDR3`
- WAL format: `BDW3`
- Published baseline retained: `v0.1.0`
- Publication gate: V96 final validation bundle must complete successfully and V97 metadata audit must remain clean.

## Candidate architecture

`key -> rho -> local Robin Hood -> fingerprint -> value`

Durability path:

`API -> ticket/group commit -> BDW3 WAL -> BDR3 checkpoint -> streaming recovery`

Additional hardening:

- process-level exclusive open lock;
- writer I/O error propagation;
- torn-tail recovery;
- deterministic sequence validation;
- checkpoint/WAL rotation;
- C++ API, C ABI v1, Python wheel;
- CMake install package.

## Promotion requirements

Do not create a tag or release until all of the following are satisfied:

1. V96 final manifest reports `candidate=true`.
2. V97 metadata audit passes.
3. V90 market benchmark completed with valid result artifacts.
4. Long soak completed without state divergence.
5. C ABI symbol set matches the freeze document.
6. Python wheel installs in a clean environment.
7. Native C/C++ consumers work after `cmake --install`.
8. License remains the BDR Academic and Non-Commercial Research License v1.0.
9. `CITATION.cff` is updated only at actual release time, never during staging.
10. No claim of production readiness or worst-case O(1) is introduced.

## Explicit non-actions

This staging work does **not**:

- create a Git tag;
- create a GitHub Release;
- create a Zenodo software record;
- update the historical v0.1.0 DOI;
- change the project license;
- merge the experimental branch into `main`.
