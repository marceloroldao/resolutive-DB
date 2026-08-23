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
pyproject = (root / 'experimental/api_v88/python/pyproject.toml').read_text(encoding='utf-8')
freeze = (root / 'experimental/api_v91/API_FREEZE_DRAFT.md').read_text(encoding='utf-8')
release_notes_path = root / 'RELEASE_NOTES_v0.2.0-rc1.md'
release_notes = release_notes_path.read_text(encoding='utf-8') if release_notes_path.exists() else ''

# Licensing model must remain explicit and source-available, non-commercial.
for needle in [
    'BDR ACADEMIC AND NON-COMMERCIAL RESEARCH LICENSE v1.0',
    'Commercial Use Prohibited Without Separate License',
    'No Patent License',
    'not represented as an OSI-approved open-source license',
]:
    if needle not in license_text:
        errors.append(f'LICENSE missing required clause: {needle}')

# Published citation metadata stays pinned to immutable v0.1.0 until a new release
# is actually tagged/published. Staging a release candidate does not rewrite history.
for needle in [
    'version: "0.1.0"',
    'doi: "10.5281/zenodo.21938148"',
    'releases/tag/v0.1.0',
    'doi: "10.5281/zenodo.21937842"',
]:
    if needle not in citation:
        errors.append(f'CITATION.cff historical baseline changed or missing: {needle}')
notes.append('CITATION.cff intentionally remains pinned to published v0.1.0 until release time.')

m = re.search(r'^name\s*=\s*"([^"]+)"', pyproject, re.M)
v = re.search(r'^version\s*=\s*"([^"]+)"', pyproject, re.M)
name = m.group(1) if m else None
version = v.group(1) if v else None

# Distinguish pre-RC experimentation from an explicitly staged release candidate.
rc_staged = (
    name == 'bdr-native'
    and version == '0.2.0rc1'
    and 'C ABI v1' in freeze
    and release_notes_path.exists()
    and 'Release Candidate' in release_notes
)

if rc_staged:
    notes.append('Release-candidate staging detected: bdr-native 0.2.0rc1 with frozen C ABI v1.')
    if 'experimental, not released' in freeze.lower():
        errors.append('Frozen RC ABI document still identifies itself as experimental/not released.')
else:
    # Before RC staging the package must remain explicitly experimental.
    if not name or 'experimental' not in name:
        errors.append('Pre-RC wheel name must remain explicitly experimental.')
    if not version or version in {'1.0.0', '0.2.0', '0.2.0rc1'}:
        errors.append('Pre-RC wheel must not use a release or release-candidate version.')
    if 'draft' not in freeze.lower():
        errors.append('Pre-RC API freeze document must identify itself as a draft.')

result = {
    'schema': 2,
    'release_metadata_ready': not errors,
    'release_candidate_staged': rc_staged,
    'published_baseline_preserved': 'version: "0.1.0"' in citation,
    'license_model': 'academic/non-commercial source-available; commercial license required',
    'python_package': {'name': name, 'version': version},
    'notes': notes,
    'errors': errors,
}
out = root / 'experimental/api_v97/metadata_audit.json'
out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, indent=2))
if errors:
    sys.exit(1)
