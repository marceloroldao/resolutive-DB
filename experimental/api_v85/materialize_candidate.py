from __future__ import annotations

import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "experimental" / "api_v85" / "generated"
BASE = ROOT / "experimental" / "api_v56"


def run(*args: str) -> None:
    subprocess.run(args, check=True)


def main() -> None:
    shutil.rmtree(OUT, ignore_errors=True)
    (OUT / "src").mkdir(parents=True)
    (OUT / "include" / "bdr").mkdir(parents=True)

    # Preserve the already-tested public C++ surface and index implementation.
    for name in ("database.hpp", "resolutive_index.hpp"):
        shutil.copy2(BASE / "include" / "bdr" / name, OUT / "include" / "bdr" / name)
    shutil.copy2(BASE / "src" / "resolutive_index.cpp", OUT / "src" / "resolutive_index.cpp")

    s0 = BASE / "src" / "database.cpp"
    s1 = OUT / "src" / "database.v74.cpp"
    s2 = OUT / "src" / "database.v75.cpp"
    sf = OUT / "src" / "database.cpp"

    run(sys.executable, str(ROOT / "experimental/api_v74/harden_writer.py"), str(s0), str(s1))
    run(sys.executable, str(ROOT / "experimental/api_v75/stream_snapshot.py"), str(s1), str(s2))
    run(sys.executable, str(ROOT / "experimental/api_v79/process_lock.py"), str(s2), str(sf))

    text = sf.read_text()
    required = [
        "std::exception_ptr writer_error",
        "replay_snapshot(",
        "LOCK_EX|LOCK_NB",
        "BDR database is already open by another process",
        "BDR fdatasync failed",
    ]
    forbidden = ["std::abort()", "decode_snapshot("]
    for needle in required:
        if needle not in text:
            raise SystemExit(f"missing integrated invariant: {needle}")
    for needle in forbidden:
        if needle in text:
            raise SystemExit(f"forbidden legacy path remains: {needle}")

    digest = hashlib.sha256(sf.read_bytes()).hexdigest()
    manifest = OUT / "MANIFEST.txt"
    manifest.write_text(
        "BDR V85 deterministic integrated candidate\n"
        f"database.cpp.sha256={digest}\n"
        "base=experimental/api_v56\n"
        "patch_order=v74_writer_error,v75_stream_recovery,v79_process_lock\n"
    )
    print(manifest.read_text(), end="")


if __name__ == "__main__":
    main()
