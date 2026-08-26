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

# v0.2.0-rc1 is now the published software baseline. Experimental v0.3 work must
# preserve that citation metadata until another release is explicitly staged and
# published. The scientific preprint remains a separate historical citation.
published_release_needles = [
    'version: "0.2.0-rc1"',
    'date-released: 2026-08-24',
    'doi: "10.5281/zenodo.22074886"',
    'releases/tag/0.2.0-rc1',
]
for needle in published_release_needles:
    if needle not in citation:
        errors.append(f'CITATION.cff published v0.2.0-rc1 baseline changed or missing: {needle}')

if 'doi: "10.5281/zenodo.21937842"' not in citation:
    errors.append('CITATION.cff associated scientific preprint DOI changed or missing.')

notes.append('CITATION.cff remains pinned to the published v0.2.0-rc1 software release during v0.3 experimentation.')
notes.append('The v0.1 research history remains preserved by its immutable tag/release; the current citation points to the latest published software baseline.')

m = re.search(r'^name\s*=\s*"([^"]+)"', pyproject, re.M)
v = re.search(r'^version\s*=\s*"([^"]+)"', pyproject, re.M)
name = m.group(1) if m else None
version = v.group(1) if v else None

# The integration package and C ABI remain on the published v0.2 RC contract while
# v0.3 scale/robustness experiments run out-of-tree from that public interface.
rc_baseline = (
    name == 'bdr-native'
    and version == '0.2.0rc1'
    and 'C ABI v1' in freeze
    and release_notes_path.exists()
    and 'Release Candidate' in release_notes
)

if rc_baseline:
    notes.append('Published release-candidate integration baseline detected: bdr-native 0.2.0rc1 with frozen C ABI v1.')
    if 'experimental, not released' in freeze.lower():
        errors.append('Frozen RC ABI document still identifies itself as experimental/not released.')
else:
    errors.append('Published v0.2.0-rc1 integration baseline is not intact.')

published_baseline_preserved = all(needle in citation for needle in published_release_needles)

result = {
    'schema': 3,
    'release_metadata_ready': not errors,
    'release_candidate_staged': rc_baseline,
    'published_baseline_preserved': published_baseline_preserved,
    'published_software_baseline': '0.2.0-rc1',
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
