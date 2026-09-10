#!/usr/bin/env python3
from pathlib import Path
import json
import re
import sys

root = Path(__file__).resolve().parents[2]
staging = root / 'experimental/api_v98'

errors = []
notes = []
stable_version = '1.1.0'
candidate_version = '1.2.0rc1'
candidate_tag_version = '1.2.0-rc1'
software_doi = '10.5281/zenodo.22130421'
previous_software_doi = '10.5281/zenodo.22120246'


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
notes.append('Pre-publication V98 staging documents are historical evidence, not statements of current release status.')

require_text('LICENSE', 'BDR ACADEMIC AND NON-COMMERCIAL RESEARCH LICENSE v1.0')
require_text('LICENSE', 'Commercial Use Prohibited Without Separate License')
require_text('LICENSE', 'No Patent License')

# Published metadata must remain pinned to v1.1.0 while v1.2.0rc1 is only a candidate.
require_text('CITATION.cff', f'version: "{stable_version}"')
require_text('CITATION.cff', f'releases/tag/v{stable_version}')
require_text('CITATION.cff', f'doi: "{software_doi}"')
require_text('CITATION.cff', '10.5281/zenodo.21937842')
require_text('RELEASE_NOTES_v1.1.0.md', 'V112 Memoria Atomic Benchmark')
require_text('CHANGELOG.md', '1.1.0 — Final / Publication Ready')
require_text('README.md', 'BDR v1.1.0 — Released')
require_text('README.md', software_doi)
require_text('README.md', previous_software_doi)

root_pyproject = (root / 'pyproject.toml').read_text(encoding='utf-8')
root_init = (root / 'bdr/__init__.py').read_text(encoding='utf-8')
name_match = re.search(r'^name\s*=\s*"([^"]+)"', root_pyproject, re.M)
version_match = re.search(r'^version\s*=\s*"([^"]+)"', root_pyproject, re.M)
root_name = name_match.group(1) if name_match else None
root_version = version_match.group(1) if version_match else None

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
    rc_path = root / 'RELEASE_NOTES_v1.2.0-rc1.md'
    if not rc_path.exists():
        errors.append('RELEASE_NOTES_v1.2.0-rc1.md is missing for the 1.2.0rc1 candidate.')
    else:
        rc_notes = rc_path.read_text(encoding='utf-8')
        for needle in [
            'release candidate under final validation',
            'BDR v1.1.0 remains the published stable baseline',
            f'`v{candidate_tag_version}`',
            'No tag, GitHub release, Zenodo record, or stable promotion should be created until the final CI round',
        ]:
            if needle not in rc_notes:
                errors.append(f'RELEASE_NOTES_v1.2.0-rc1.md missing RC safety evidence: {needle}')
    notes.append('v1.2.0rc1 candidate staging is active while published v1.1.0 citation/Zenodo metadata remains authoritative.')
else:
    notes.append('Root package remains on the published v1.1.0 stable line.')

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
if zenodo.get('version') != stable_version:
    errors.append(f'.zenodo.json is not published stable version {stable_version}')
related = zenodo.get('related_identifiers', [])
if not any(x.get('identifier') == software_doi and x.get('relation') == 'isIdenticalTo' for x in related):
    errors.append('.zenodo.json does not identify the definitive v1.1.0 DOI')
if not any(x.get('identifier') == previous_software_doi and x.get('relation') == 'isNewVersionOf' for x in related):
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
    'schema': 6,
    'historical_staging_version': '0.2.0-rc1',
    'previous_released_version': '1.0.0',
    'published_stable_version': stable_version,
    'candidate_version': candidate_version if candidate_mode else None,
    'publication_target': candidate_tag_version if candidate_mode else stable_version,
    'publication_state': 'release-candidate' if candidate_mode else 'released',
    'software_doi': software_doi,
    'previous_software_doi': previous_software_doi,
    'candidate_evidence_present': manifest_path.exists(),
    'candidate': candidate,
    'staging_history_preserved': True,
    'v11_publication_metadata_finalized': True,
    'staging_safe': not errors,
    'notes': notes,
    'errors': errors,
}
(staging / 'staging_audit.json').write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(out, indent=2))

if errors:
    sys.exit(1)
