#!/usr/bin/env python3
import json, hashlib, os, subprocess, sys, pathlib, time

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "experimental" / "api_v92" / "out"
OUT.mkdir(parents=True, exist_ok=True)

checks = []

def run(name, cmd, cwd=None):
    t0 = time.time()
    p = subprocess.run(cmd, cwd=cwd or ROOT, shell=True, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log = OUT / f"{name}.log"
    log.write_text(p.stdout, encoding="utf-8")
    checks.append({"name": name, "ok": p.returncode == 0,
                   "returncode": p.returncode, "seconds": round(time.time()-t0, 3),
                   "log": str(log.relative_to(ROOT))})
    if p.returncode != 0:
        print(f"FAIL: {name}\n{p.stdout}")
    return p.returncode == 0

def sha256(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()

commit = subprocess.check_output("git rev-parse HEAD", shell=True, text=True, cwd=ROOT).strip()

# 1) Consolidated C++ core
run("v86_configure", "cmake -S experimental/api_v86 -B build/v92_v86 -DCMAKE_BUILD_TYPE=Release")
run("v86_build", "cmake --build build/v92_v86 -j2")
run("v86_ctest", "ctest --test-dir build/v92_v86 --output-on-failure")

# 2) C ABI v1
run("v87_configure", "cmake -S experimental/api_v87 -B build/v92_v87 -DCMAKE_BUILD_TYPE=Release")
run("v87_build", "cmake --build build/v92_v87 -j2")
run("v87_ctest", "ctest --test-dir build/v92_v87 --output-on-failure")

# Stable exported ABI symbols
expected = {
"bdr_abi_version","bdr_options_init","bdr_open","bdr_close","bdr_put","bdr_put_sync",
"bdr_delete","bdr_delete_sync","bdr_get","bdr_wait","bdr_sync","bdr_checkpoint",
"bdr_last_sequence","bdr_durable_sequence","bdr_size","bdr_last_error"}
so_candidates=list((ROOT/"build/v92_v87").glob("**/libbdr_c.so*"))
abi_symbols_ok=False
abi_symbols=[]
if so_candidates:
    so=max(so_candidates, key=lambda p: len(p.name))
    p=subprocess.run(f"nm -D --defined-only {so} | awk '{{print $3}}'", shell=True, text=True,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    abi_symbols=sorted(set(x.strip() for x in p.stdout.splitlines() if x.strip()).intersection(expected))
    abi_symbols_ok = set(abi_symbols)==expected
checks.append({"name":"v87_abi_symbols","ok":abi_symbols_ok,
               "found":abi_symbols,"expected":sorted(expected)})

# 3) Python wheel smoke test. The workflow stages the shared library into the package first.
run("v88_python_install", "python3 -m pip install -q --upgrade pip build wheel && python3 -m pip install -q experimental/api_v88/python")
run("v88_python_contract", "python3 experimental/api_v88/test_v88.py")

# 4) Freeze invariants in source tree
source=(ROOT/"experimental/api_v86/src/database.cpp").read_text(encoding="utf-8")
invariants = {
    "no_abort": "std::abort()" not in source,
    "writer_error": "writer_error" in source,
    "streaming_snapshot": "replay_snapshot" in source,
    "process_lock": "flock" in source,
    "bdw3": "BDW3" in source,
    "bdr3": "BDR3" in source,
}
for k,v in invariants.items(): checks.append({"name":f"invariant_{k}","ok":bool(v)})

# 5) Hash key candidate artifacts
artifact_paths=[
    ROOT/"experimental/api_v86/src/database.cpp",
    ROOT/"experimental/api_v86/src/resolutive_index.cpp",
    ROOT/"experimental/api_v87/include/bdr/bdr_c.h",
    ROOT/"experimental/api_v87/src/bdr_c.cpp",
]
artifacts=[]
for p in artifact_paths:
    artifacts.append({"path":str(p.relative_to(ROOT)),"sha256":sha256(p)})

candidate = all(c.get("ok",False) for c in checks)
manifest={
    "schema":"bdr-release-candidate-audit-v1",
    "commit":commit,
    "candidate":candidate,
    "abi_version":1,
    "disk_formats":{"snapshot":"BDR3","wal":"BDW3"},
    "checks":checks,
    "artifacts":artifacts,
}
(OUT/"manifest.json").write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n",encoding="utf-8")

md=["# BDR V92 Release-Candidate Audit","",f"- Commit: `{commit}`",f"- Candidate: **{candidate}**","- C ABI: `1`","- Snapshot/WAL: `BDR3` / `BDW3`","","## Checks",""]
for c in checks:
    md.append(f"- {'PASS' if c.get('ok') else 'FAIL'} — `{c['name']}`")
md += ["","## Artifact SHA-256",""]
for a in artifacts: md.append(f"- `{a['sha256']}`  `{a['path']}`")
(OUT/"REPORT.md").write_text("\n".join(md)+"\n",encoding="utf-8")
print(json.dumps(manifest, indent=2))
sys.exit(0 if candidate else 1)
