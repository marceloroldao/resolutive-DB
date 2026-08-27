#!/usr/bin/env python3
import json
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[2]
errors = []
notes = []

license_text = (root / 'LICENSE').read_text(encoding='utf-8')
citation = (root / 'CITATION.cff').read_text(encoding='utf-8')
root_pyproject = (root / 'pyproject.toml').read_text(encoding='utf-8')
native_pyproject = (root / 'experimental/api_v88/python/pyproject.toml').read_text(encoding='utf-8')
freeze = (root / 'experimental/api_v91/API_FREEZE_DRAFT.md').read_text(encoding='utf-8')
rc_release_notes_path = root / 'RELEASE_NOTES_v0.2.0-rc1.md'
v1_release_notes_path = root / 'RELEASE_NOTES_v1.0.0.md'

for needle in [
    'BDR ACADEMIC AND NON-COMMERCIAL RESEARCH LICENSE v1.0',
    'Commercial Use Prohibited Without Separate License',
    'No Patent License',
    'not represented as an OSI-approved open-source license',
]:
    if needle not in license_text:
        errors.append(f'LICENSE missing required clause: {needle}')

# The publication branch must identify v1.0.0, while never predeclaring a
# software DOI. The only DOI allowed in CITATION.cff before publication is the
# already-real associated scientific preprint DOI in preferred-citation.
for needle in [
    'version: "1.0.0"',
    'repository-code: "https://github.com/marceloroldao/resolutive-DB"',
    'doi: "10.5281/zenodo.21937842"',
]:
    if needle not in citation:
        errors.append(f'CITATION.cff v1 publication metadata missing: {needle}')

for forbidden in [
    'doi: "10.5281/zenodo.22074886"',
    'releases/tag/0.2.0-rc1',
    'version: "0.2.0-rc1"',
]:
    if forbidden in citation:
        errors.append(f'CITATION.cff still contains superseded software-release metadata: {forbidden}')

root_name = re.search(r'^name\s*=\s*"([^"]+)"', root_pyproject, re.M)
root_version = re.search(r'^version\s*=\s*"([^"]+)"', root_pyproject, re.M)
root_name = root_name.group(1) if root_name else None
root_version = root_version.group(1) if root_version else None
if root_name != 'resolutive-db' or root_version != '1.0.0':
    errors.append(f'Root package metadata is not v1.0.0: name={root_name!r} version={root_version!r}')

# Preserve the historical native RC integration fixture exactly as published;
# it is evidence, not the v1 package version source of truth.
native_name = re.search(r'^name\s*=\s*"([^"]+)"', native_pyproject, re.M)
native_version = re.search(r'^version\s*=\s*"([^"]+)"', native_pyproject, re.M)
native_name = native_name.group(1) if native_name else None
native_version = native_version.group(1) if native_version else None
historical_rc_intact = (
    native_name == 'bdr-native'
    and native_version == '0.2.0rc1'
    and 'C ABI v1' in freeze
    and rc_release_notes_path.exists()
)
if not historical_rc_intact:
    errors.append('Published v0.2.0-rc1 historical integration evidence is not intact.')

if not v1_release_notes_path.exists():
    errors.append('Final RELEASE_NOTES_v1.0.0.md is missing.')
else:
    v1_notes = v1_release_notes_path.read_text(encoding='utf-8')
    for needle in ['PUBLICATION READY / TAG PENDING', '50,000,000-operation materialized soak: PASS']:
        if needle not in v1_notes:
            errors.append(f'RELEASE_NOTES_v1.0.0.md missing required publication evidence: {needle}')

notes.append('CITATION.cff is staged for v1.0.0 without a predeclared software DOI.')
notes.append('The real v0.2.0-rc1 integration baseline remains preserved as historical evidence.')
notes.append('The associated scientific preprint DOI remains unchanged and real.')

result = {
    'schema': 4,
    'release_metadata_ready': not errors,
    'publication_target': '1.0.0',
    'software_doi_predeclared': 'doi: "10.5281/zenodo.22074886"' in citation,
    'historical_rc_integration_preserved': historical_rc_intact,
    'published_software_baseline': '0.2.0-rc1',
    'root_package': {'name': root_name, 'version': root_version},
    'historical_native_package': {'name': native_name, 'version': native_version},
    'license_model': 'academic/non-commercial source-available; commercial license required',
    'notes': notes,
    'errors': errors,
}
out = root / 'experimental/api_v97/metadata_audit.json'
out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, indent=2))
if errors:
    sys.exit(1)
