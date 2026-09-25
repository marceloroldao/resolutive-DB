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
zenodo = json.loads((root / '.zenodo.json').read_text(encoding='utf-8'))
root_pyproject = (root / 'pyproject.toml').read_text(encoding='utf-8')
root_init = (root / 'bdr/__init__.py').read_text(encoding='utf-8')
native_pyproject = (root / 'experimental/api_v88/python/pyproject.toml').read_text(encoding='utf-8')
freeze = (root / 'experimental/api_v91/API_FREEZE_DRAFT.md').read_text(encoding='utf-8')
rc_release_notes_path = root / 'RELEASE_NOTES_v0.2.0-rc1.md'
v1_release_notes_path = root / 'RELEASE_NOTES_v1.0.0.md'
v11_release_notes_path = root / 'RELEASE_NOTES_v1.1.0.md'
v12_rc1_release_notes_path = root / 'RELEASE_NOTES_v1.2.0-rc4.md'

stable_version = '1.1.0'
candidate_version = '1.2.0rc4'
candidate_tag_version = '1.2.0-rc4'
software_doi = '10.5281/zenodo.22130421'
previous_software_doi = '10.5281/zenodo.22120246'
preprint_doi = '10.5281/zenodo.21937842'
rc4_doi = '10.5281/zenodo.22948288'

for needle in [
    'BDR ACADEMIC AND NON-COMMERCIAL RESEARCH LICENSE v1.0',
    'Commercial Use Prohibited Without Separate License',
    'No Patent License',
    'not represented as an OSI-approved open-source license',
]:
    if needle not in license_text:
        errors.append(f'LICENSE missing required clause: {needle}')

# The primary citation remains the stable baseline; a reference identifies the
# published RC4 independently, without rewriting the stable software DOI.
for needle in [
    f'version: "{stable_version}"',
    'repository-code: "https://github.com/marceloroldao/resolutive-DB"',
    f'url: "https://github.com/marceloroldao/resolutive-DB/releases/tag/v{stable_version}"',
    f'doi: "{software_doi}"',
    f'doi: "{preprint_doi}"',
]:
    if needle not in citation:
        errors.append(f'CITATION.cff published v1.1 metadata missing: {needle}')

for forbidden in [
    f'doi: "{previous_software_doi}"',
    'doi: "10.5281/zenodo.22074886"',
    'version: "1.0.0"',
    'version: "0.2.0-rc1"',
]:
    if forbidden in citation:
        errors.append(f'CITATION.cff contains stale/current-version software metadata: {forbidden}')

if f'doi: "{rc4_doi}"' not in citation or 'version: "1.2.0-rc4"' not in citation:
    errors.append('CITATION.cff does not reference published RC4 DOI and version')

root_name_match = re.search(r'^name\s*=\s*"([^"]+)"', root_pyproject, re.M)
root_version_match = re.search(r'^version\s*=\s*"([^"]+)"', root_pyproject, re.M)
root_name = root_name_match.group(1) if root_name_match else None
root_version = root_version_match.group(1) if root_version_match else None

if root_name != 'resolutive-db':
    errors.append(f'Root package name is not resolutive-db: {root_name!r}')

candidate_mode = root_version == candidate_version
stable_mode = root_version == stable_version
if not (candidate_mode or stable_mode):
    errors.append(
        f'Root package version is neither published stable {stable_version!r} '
        f'nor approved candidate {candidate_version!r}: {root_version!r}'
    )

expected_init_version = candidate_version if candidate_mode else stable_version
if f'__version__ = "{expected_init_version}"' not in root_init:
    errors.append(f'bdr.__version__ is not aligned with root package {expected_init_version}')

if candidate_mode:
    if not v12_rc1_release_notes_path.exists():
        errors.append('RELEASE_NOTES_v1.2.0-rc4.md is missing for the 1.2.0rc4 candidate.')
    else:
        rc_notes = v12_rc1_release_notes_path.read_text(encoding='utf-8')
        for needle in [
            'GitHub pre-release and Zenodo record published',
            'BDR v1.1.0 remains the published stable baseline',
            f'`v{candidate_tag_version}`',
            'The tag and Zenodo record are published'
        ]:
            if needle not in rc_notes:
                errors.append(f'RELEASE_NOTES_v1.2.0-rc4.md missing RC safety evidence: {needle}')
    notes.append(
        'Root package is the published v1.2.0rc4 candidate; the stable v1.1.0 primary citation remains intact and the RC4 DOI is referenced.'
    )
else:
    notes.append('Root package matches the published v1.1.0 stable release.')

if zenodo.get('version') != candidate_tag_version or zenodo.get('doi') != rc4_doi:
    errors.append('.zenodo.json must identify the published RC4 version and DOI')
if zenodo.get('title') != 'Banco de Dados Resolutivo (BDR) / Resolutive Database Engine — v1.2.0 Release Candidate 4':
    errors.append('.zenodo.json RC4 title mismatch')
if 'license' in zenodo:
    errors.append('Custom BDR license must not be encoded as an unsupported Zenodo identifier')
related = zenodo.get('related_identifiers') or []
if not any(x.get('identifier') == software_doi and x.get('relation') == 'isNewVersionOf' for x in related):
    errors.append('.zenodo.json does not identify the stable v1.1.0 software DOI as RC4 predecessor')
if any(x.get('relation') == 'isIdenticalTo' for x in related):
    errors.append('.zenodo.json must not claim identity with an earlier version')
if not any(x.get('identifier') == preprint_doi for x in related):
    errors.append('.zenodo.json does not preserve the scientific preprint relation')

native_name_match = re.search(r'^name\s*=\s*"([^"]+)"', native_pyproject, re.M)
native_version_match = re.search(r'^version\s*=\s*"([^"]+)"', native_pyproject, re.M)
native_name = native_name_match.group(1) if native_name_match else None
native_version = native_version_match.group(1) if native_version_match else None
historical_rc_intact = (
    native_name == 'bdr-native'
    and native_version == '0.2.0rc1'
    and 'C ABI v1' in freeze
    and rc_release_notes_path.exists()
)
if not historical_rc_intact:
    errors.append('Published v0.2.0-rc1 historical integration evidence is not intact.')

if not v1_release_notes_path.exists():
    errors.append('Historical RELEASE_NOTES_v1.0.0.md is missing.')
if not v11_release_notes_path.exists():
    errors.append('RELEASE_NOTES_v1.1.0.md is missing.')
else:
    notes_text = v11_release_notes_path.read_text(encoding='utf-8')
    for needle in ['V112 Memoria Atomic Benchmark', 'V100 Evidence Closure']:
        if needle not in notes_text:
            errors.append(f'RELEASE_NOTES_v1.1.0.md missing publication evidence: {needle}')

notes.append(f'BDR v1.1.0 definitive software DOI is {software_doi}.')
notes.append(f'BDR v1.2.0-rc4 DOI is {rc4_doi}.')
notes.append(f'Published v1.0.0 software DOI {previous_software_doi} is retained as prior-version provenance.')
notes.append('The associated scientific preprint DOI remains unchanged.')

result = {
    'schema': 7,
    'release_metadata_ready': not errors,
    'published_stable_version': stable_version,
    'candidate_version': candidate_version if candidate_mode else None,
    'publication_target': candidate_tag_version if candidate_mode else stable_version,
    'publication_state': 'release-candidate' if candidate_mode else 'released',
    'software_doi': software_doi,
    'candidate_doi': rc4_doi,
    'previous_software_doi': previous_software_doi,
    'historical_rc_integration_preserved': historical_rc_intact,
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
