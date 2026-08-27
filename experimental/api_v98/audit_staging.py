#!/usr/bin/env python3
from pathlib import Path
import json
import sys

root = Path(__file__).resolve().parents[2]
staging = root / 'experimental/api_v98'

errors = []
notes = []

def require_text(path, needle):
    p = root / path
    if not p.exists():
        errors.append(f'missing: {path}')
        return
    text = p.read_text(encoding='utf-8')
    if needle not in text:
        errors.append(f'{path}: missing {needle!r}')

require_text('experimental/api_v98/RC_STAGING.md', 'NOT RELEASED / NOT TAGGED / NOT PUBLISHED')
require_text('experimental/api_v98/RELEASE_NOTES_v0.2.0-rc1_DRAFT.md', 'Draft only. Not released.')
notes.append('Pre-publication V98 staging documents are retained as historical evidence and are not statements of current release status.')

require_text('LICENSE', 'BDR ACADEMIC AND NON-COMMERCIAL RESEARCH LICENSE v1.0')
require_text('LICENSE', 'Commercial Use Prohibited Without Separate License')
require_text('LICENSE', 'No Patent License')

require_text('CITATION.cff', 'version: "0.2.0-rc1"')
require_text('CITATION.cff', 'date-released: 2026-08-24')
require_text('CITATION.cff', '10.5281/zenodo.22074886')
require_text('CITATION.cff', 'releases/tag/0.2.0-rc1')
require_text('CITATION.cff', '10.5281/zenodo.21937842')

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
    'schema': 2,
    'historical_staging_version': '0.2.0-rc1',
    'published_software_baseline': '0.2.0-rc1',
    'candidate_evidence_present': manifest_path.exists(),
    'candidate': candidate,
    'staging_history_preserved': True,
    'staging_safe': not errors,
    'notes': notes,
    'errors': errors,
}
(staging / 'staging_audit.json').write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(out, indent=2))

if errors:
    sys.exit(1)
