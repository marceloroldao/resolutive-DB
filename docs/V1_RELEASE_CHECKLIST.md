# BDR v1.0.0 — Release Checklist

Status: **publication-ready / tag pending**

This checklist is the final gate before the v1.0.0 tag, GitHub Release and DOI registration.

## Technical closure

- [x] v1 candidate promoted selectively to `main`.
- [x] post-merge `main` BDR CI PASS.
- [x] PR-head tree and merge tree are byte-identical for the technical candidate.
- [x] public C++ API lock PASS.
- [x] root-level CMake configure/build PASS.
- [x] root-level package install + external consumer PASS.
- [x] BDR3/BDW3 compatibility PASS.
- [x] 12 checkpoint crash failpoints PASS.
- [x] 200 checkpoint/reopen cycles PASS on candidate and fallback.
- [x] 50M-operation soak PASS.
- [x] V99 evidence closure PASS.
- [x] V100 evidence closure PASS.

## Public release metadata

- [x] `README.md` switched to v1.0.0 publication-ready status.
- [x] `CITATION.cff` switched to v1.0.0 without inventing a software DOI.
- [x] `.zenodo.json` switched to v1.0.0 metadata.
- [x] package/version metadata switched to v1.0.0 where applicable.
- [x] final `RELEASE_NOTES_v1.0.0.md` created and draft removed.
- [x] `CHANGELOG.md` changed from `Unreleased / Finalization` to final publication-ready status.
- [ ] publication branch CI PASS.
- [ ] exact publication metadata tree merged to `main`.
- [ ] final post-merge `main` CI PASS.

## Publication sequence

1. validate this final metadata tree on the publication branch;
2. merge that exact tree into `main`;
3. rerun final CI on `main`;
4. create the `v1.0.0` tag/GitHub Release only from that validated commit;
5. register/update DOI metadata only with the actual DOI returned by the publication service;
6. add the real v1 software DOI to README/CITATION metadata after publication if needed;
7. verify GitHub/Zenodo metadata and citations after publication.

## Hard rules

- Never invent or predict a DOI.
- Never claim v1 is publicly released before the tag/release exists.
- Do not change BDR3/BDW3 formats during metadata finalization.
- Do not change the frozen public API during metadata finalization.
- Any code change after final technical closure requires re-running the relevant technical gates.
