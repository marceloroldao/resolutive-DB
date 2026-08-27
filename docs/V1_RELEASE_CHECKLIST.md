# BDR v1.0.0 — Release Checklist

Status: **finalization / not published**

This checklist is the final gate before any v1 tag, GitHub Release, DOI registration, or public metadata switch.

## Technical closure

- [x] v1 candidate promoted selectively to `main`.
- [x] post-merge `main` BDR CI PASS.
- [x] PR-head tree and merge tree are byte-identical.
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

The following remain intentionally on v0.2.0-rc1 until the final publication commit:

- [ ] `README.md` switched to v1.0.0 status.
- [ ] `CITATION.cff` switched to v1.0.0 and contains only real publication identifiers.
- [ ] `.zenodo.json` switched to v1.0.0 metadata.
- [ ] package/version metadata switched to v1.0.0 where applicable.
- [ ] `RELEASE_NOTES_v1.0.0_DRAFT.md` promoted to final release notes.
- [ ] `CHANGELOG.md` changed from `Unreleased / Finalization` to final release date/status.

## Publication sequence

1. complete and validate the final metadata commit on the release branch;
2. merge that exact tree into `main`;
3. rerun final CI on `main`;
4. create the v1.0.0 tag/release only from that validated commit;
5. register/update DOI metadata only with the actual DOI returned by the publication service;
6. verify GitHub/Zenodo metadata and citations after publication.

## Hard rules

- Never invent or predict a DOI.
- Never claim v1 is published before the tag/release exists.
- Do not change BDR3/BDW3 formats during metadata finalization.
- Do not change the frozen public API during metadata finalization.
- Any code change after final technical closure requires re-running the relevant technical gates.
