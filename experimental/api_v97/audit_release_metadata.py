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

# Licensing model must remain explicit and source-available, non-commercial.
for needle in [
    'BDR ACADEMIC AND NON-COMMERCIAL RESEARCH LICENSE v1.0',
    'Commercial Use Prohibited Without Separate License',
    'No Patent License',
    'not represented as an OSI-approved open-source license',
]:
    if needle not in license_text:
        errors.append(f'LICENSE missing required clause: {needle}')

# Published citation metadata must stay pinned to the immutable v0.1.0 baseline
# until a new release is actually created.
for needle in [
    'version: "0.1.0"',
    'doi: "10.5281/zenodo.21938148"',
    'releases/tag/v0.1.0',
    'doi: "10.5281/zenodo.21937842"',
]:
    if needle not in citation:
        errors.append(f'CITATION.cff historical baseline changed or missing: {needle}')
notes.append('CITATION.cff intentionally remains pinned to published v0.1.0 until release time.')

# Experimental wheel must not masquerade as the final public package/version.
m = re.search(r'^name\s*=\s*"([^"]+)"', pyproject, re.M)
v = re.search(r'^version\s*=\s*"([^"]+)"', pyproject, re.M)
name = m.group(1) if m else None
version = v.group(1) if v else None
if not name or 'experimental' not in name:
    errors.append('Experimental wheel name must remain explicitly experimental before release.')
if not version or version in {'1.0.0', '0.2.0'}:
    errors.append('Experimental wheel must not use the future public release version before release.')

# Freeze document must still be a draft before evidence closes.
if 'draft' not in freeze.lower():
    errors.append('API freeze document no longer identifies itself as a draft.')

result = {
    'schema': 1,
    'release_metadata_ready': not errors,
    'published_baseline_preserved': 'version: "0.1.0"' in citation,
    'license_model': 'academic/non-commercial source-available; commercial license required',
    'experimental_python_package': {'name': name, 'version': version},
    'notes': notes,
    'errors': errors,
}
out = root / 'experimental/api_v97/metadata_audit.json'
out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, indent=2))
if errors:
    sys.exit(1)
