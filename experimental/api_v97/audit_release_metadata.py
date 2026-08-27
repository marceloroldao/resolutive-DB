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

software_doi = '10.5281/zenodo.22130421'
previous_software_doi = '10.5281/zenodo.22120246'
preprint_doi = '10.5281/zenodo.21937842'

for needle in [
    'BDR ACADEMIC AND NON-COMMERCIAL RESEARCH LICENSE v1.0',
    'Commercial Use Prohibited Without Separate License',
    'No Patent License',
    'not represented as an OSI-approved open-source license',
]:
    if needle not in license_text:
        errors.append(f'LICENSE missing required clause: {needle}')

for needle in [
    'version: "1.1.0"',
    'repository-code: "https://github.com/marceloroldao/resolutive-DB"',
    'url: "https://github.com/marceloroldao/resolutive-DB/releases/tag/v1.1.0"',
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

root_name = re.search(r'^name\s*=\s*"([^"]+)"', root_pyproject, re.M)
root_version = re.search(r'^version\s*=\s*"([^"]+)"', root_pyproject, re.M)
root_name = root_name.group(1) if root_name else None
root_version = root_version.group(1) if root_version else None
if root_name != 'resolutive-db' or root_version != '1.1.0':
    errors.append(f'Root package metadata is not v1.1.0: name={root_name!r} version={root_version!r}')
if '__version__ = "1.1.0"' not in root_init:
    errors.append('bdr.__version__ is not 1.1.0')

if zenodo.get('version') != '1.1.0':
    errors.append(f'.zenodo.json version is not 1.1.0: {zenodo.get("version")!r}')
related = zenodo.get('related_identifiers') or []
if not any(x.get('identifier') == software_doi and x.get('relation') == 'isIdenticalTo' for x in related):
    errors.append('.zenodo.json does not identify the definitive v1.1.0 software DOI')
if not any(x.get('identifier') == previous_software_doi and x.get('relation') == 'isNewVersionOf' for x in related):
    errors.append('.zenodo.json does not relate v1.1.0 to the published v1.0.0 DOI with isNewVersionOf')
if not any(x.get('identifier') == preprint_doi for x in related):
    errors.append('.zenodo.json does not preserve the scientific preprint relation')

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
    errors.append('Historical RELEASE_NOTES_v1.0.0.md is missing.')
if not v11_release_notes_path.exists():
    errors.append('RELEASE_NOTES_v1.1.0.md is missing.')
else:
    notes_text = v11_release_notes_path.read_text(encoding='utf-8')
    for needle in ['V112 Memoria Atomic Benchmark', 'V100 Evidence Closure']:
        if needle not in notes_text:
            errors.append(f'RELEASE_NOTES_v1.1.0.md missing publication evidence: {needle}')

notes.append(f'BDR v1.1.0 definitive software DOI is {software_doi}.')
notes.append(f'Published v1.0.0 software DOI {previous_software_doi} is retained as prior-version provenance.')
notes.append('The associated scientific preprint DOI remains unchanged.')

result = {
    'schema': 7,
    'release_metadata_ready': not errors,
    'publication_target': '1.1.0',
    'publication_state': 'released',
    'software_doi': software_doi,
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
