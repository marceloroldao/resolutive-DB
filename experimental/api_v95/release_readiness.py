#!/usr/bin/env python3
import hashlib, json, pathlib, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
out = ROOT / 'experimental/api_v95/out'
out.mkdir(parents=True, exist_ok=True)
checks = {}
def check(name, cond, detail=''):
    checks[name] = {'pass': bool(cond), 'detail': detail}
    if not cond:
        print('FAIL', name, detail)

def sha256(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for c in iter(lambda:f.read(1<<20), b''): h.update(c)
    return h.hexdigest()

required = [
 'experimental/api_v86/src/database.cpp',
 'experimental/api_v86/src/resolutive_index.cpp',
 'experimental/api_v87/include/bdr/bdr_c.h',
 'experimental/api_v87/src/bdr_c.cpp',
 'experimental/api_v88/python/pyproject.toml',
 'experimental/api_v91/API_FREEZE_DRAFT.md',
 'experimental/api_v93/README.md',
 'experimental/api_v94/CMakeLists.txt',
]
for p in required:
    check('exists:'+p, (ROOT/p).exists())

core=(ROOT/'experimental/api_v86/src/database.cpp').read_text()
check('no_abort', 'std::abort()' not in core)
check('writer_error', 'writer_error' in core)
check('process_lock', 'flock' in core)
check('bdr3', 'BDR3' in core)
check('bdw3', 'BDW3' in core)
check('streaming_recovery', 'decode_snapshot(' not in core and ('replay_snapshot' in core or 'stream' in core.lower()))

hdr=(ROOT/'experimental/api_v87/include/bdr/bdr_c.h').read_text()
for sym in ['bdr_open','bdr_close','bdr_put','bdr_put_sync','bdr_get','bdr_delete','bdr_delete_sync','bdr_wait','bdr_sync','bdr_checkpoint','bdr_abi_version']:
    check('abi:'+sym, sym in hdr)
check('abi_version_1', '#define BDR_C_ABI_VERSION 1u' in hdr)
check('abi_struct_size', 'struct_size' in hdr)

hashes={p:sha256(ROOT/p) for p in required if (ROOT/p).is_file()}
candidate = all(v['pass'] for v in checks.values())
manifest={'candidate':candidate,'checks':checks,'sha256':hashes}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True))
with open(out/'REPORT.md','w') as f:
    f.write('# BDR V95 Release Readiness Matrix\n\n')
    f.write(f'**candidate: {str(candidate).lower()}**\n\n')
    for k,v in checks.items():
        f.write(f"- {'PASS' if v['pass'] else 'FAIL'} — `{k}`")
        if v['detail']: f.write(f" — {v['detail']}")
        f.write('\n')
print(json.dumps(manifest,indent=2))
sys.exit(0 if candidate else 1)
