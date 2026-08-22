# V97 — Release Metadata Plan (Do Not Apply Yet)

This plan is intentionally not applied while the API/ABI remains experimental.

## Preconditions

Only prepare a new public release after all of the following are true:

1. V96 final validation bundle completes successfully.
2. `final_manifest.json` reports `candidate: true`.
3. V97 metadata audit reports `release_metadata_ready: true`.
4. API/ABI freeze is accepted and no incompatible symbol/struct changes remain.
5. Final release version is chosen explicitly.

## At release time

Update together in one release-preparation commit:

- `CITATION.cff` software `version` and `date-released`;
- release URL/tag;
- software DOI only after the new Zenodo software record exists;
- release notes and changelog;
- public package version/name if the experimental Python package is promoted;
- documentation references that currently say experimental/draft.

## Preserve historical objects

Do not overwrite or repurpose:

- software release v0.1.0;
- software DOI `10.5281/zenodo.21938148`;
- scientific preprint DOI `10.5281/zenodo.21937842`.

A future API release must be a new citable software object/version. The scientific preprint remains a separate citable object.

## Licensing

Keep the software under the BDR Academic and Non-Commercial Research License v1.0 unless a deliberate licensing decision is made separately. Commercial/production/SaaS/proprietary use continues to require a separate written commercial license. The manuscript license remains separate.

## Publication guard

Do not create a GitHub release, tag, Zenodo release, or new DOI from this plan automatically. Publication is a separate explicit action after validation.
