#!/usr/bin/env python3
from pathlib import Path
import json
import sys

root = Path(__file__).resolve().parents[2]
staging = root / 'experimental/api_v98'

errors = []
notes = []
previous_software_doi = '10.5281/zenodo.22120246'

def require_text(path, needle):
    p = root / path
    if not p.exists():
        errors.append(f'missing: {path}')
        return
    text = p.read_text(encoding='utf-8')
    if needle not in text:
        errors.append(f'{path}: missing {needle!r}')

# Historical staging evidence remains historical and unchanged.
require_text('experimental/api_v98/RC_STAGING.md', 'NOT RELEASED / NOT TAGGED / NOT PUBLISHED')
require_text('experimental/api_v98/RELEASE_NOTES_v0.2.0-rc1_DRAFT.md', 'Draft only. Not released.')
notes.append('Pre-publication V98 staging documents are historical evidence, not statements of current release status.')

require_text('LICENSE', 'BDR ACADEMIC AND NON-COMMERCIAL RESEARCH LICENSE v1.0')
require_text('LICENSE', 'Commercial Use Prohibited Without Separate License')
require_text('LICENSE', 'No Patent License')

require_text('CITATION.cff', 'version: "1.1.0"')
require_text('CITATION.cff', 'releases/tag/v1.1.0')
require_text('CITATION.cff', '10.5281/zenodo.21937842')
require_text('RELEASE_NOTES_v1.1.0.md', 'publication ready / tag pending')
require_text('RELEASE_NOTES_v1.1.0.md', 'V112 Memoria Atomic Benchmark')
require_text('CHANGELOG.md', '1.1.0 — Final / Publication Ready')
require_text('README.md', 'BDR v1.1.0 — Publication Ready / Tag Pending')
require_text('README.md', previous_software_doi)
require_text('pyproject.toml', 'version = "1.1.0"')
require_text('bdr/__init__.py', '__version__ = "1.1.0"')

citation = (root / 'CITATION.cff').read_text(encoding='utf-8')
for forbidden in [
    f'doi: "{previous_software_doi}"',
    'doi: "10.5281/zenodo.22074886"',
    'version: "1.0.0"',
    'version: "0.2.0-rc1"',
]:
    if forbidden in citation:
        errors.append(f'CITATION.cff contains stale/current-version software metadata: {forbidden}')

zenodo = json.loads((root / '.zenodo.json').read_text(encoding='utf-8'))
if zenodo.get('version') != '1.1.0':
    errors.append('.zenodo.json is not staged for version 1.1.0')
if not any(x.get('identifier') == previous_software_doi and x.get('relation') == 'isNewVersionOf' for x in zenodo.get('related_identifiers', [])):
    errors.append('.zenodo.json does not identify v1.0.0 as the prior software version')

manifest_path = root / 'v96_out/final_manifest.json'
if manifest_path.exists():
    try:
        manifest = json.loads(manifest_path.read_text())
        candidate = bool(manifest.get('candidate'))
    except Exception as exc:
        errors.append(f'cannot parse V96 manifest: {exc}')
        candidate = False
else:
    candidate = False

out = {
    'schema': 5,
    'historical_staging_version': '0.2.0-rc1',
    'previous_released_version': '1.0.0',
    'publication_target': '1.1.0',
    'publication_state': 'publication-ready-tag-pending',
    'software_doi': None,
    'previous_software_doi': previous_software_doi,
    'candidate_evidence_present': manifest_path.exists(),
    'candidate': candidate,
    'staging_history_preserved': True,
    'v11_publication_metadata_staged': True,
    'staging_safe': not errors,
    'notes': notes,
    'errors': errors,
}
(staging / 'staging_audit.json').write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(out, indent=2))

if errors:
    sys.exit(1)
