from __future__ import annotations

import argparse
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from bdr_native import Database


def child_mode(dbdir: str, progress_file: str) -> int:
    progress = Path(progress_file)
    with Database(dbdir, wal_batch=512, partition_count=4096) as db:
        last = 0
        for i in range(2_000_000):
            ticket = db.put(f"K_{i:010d}".encode(), f"V_{i:010d}".encode())
            if (i + 1) % 128 == 0:
                db.wait(ticket)
                last = ticket
                progress.write_text(str(last), encoding="ascii")
    return 0


def parent_mode(kill_ms: list[int]) -> int:
    print("kill_ms,child_signal,progress_ticket,recovered_size,last_sequence,durable_sequence,continuation_ok,pass")
    failures = 0

    with tempfile.TemporaryDirectory(prefix="bdr-v64-") as td:
        root = Path(td)
        for ms in kill_ms:
            dbdir = root / f"db-{ms}"
            progress = root / f"progress-{ms}.txt"
            shutil.rmtree(dbdir, ignore_errors=True)

            env = os.environ.copy()
            cmd = [sys.executable, __file__, "--child", str(dbdir), str(progress)]
            proc = subprocess.Popen(cmd, env=env)
            deadline = time.time() + 10
            while time.time() < deadline and not progress.exists():
                if proc.poll() is not None:
                    raise RuntimeError(f"child exited before first durable group: rc={proc.returncode}")
                time.sleep(0.001)

            time.sleep(ms / 1000.0)
            os.kill(proc.pid, signal.SIGKILL)
            rc = proc.wait(timeout=10)
            child_signal = -rc if rc < 0 else 0
            progress_ticket = int(progress.read_text(encoding="ascii")) if progress.exists() else 0

            with Database(dbdir, wal_batch=512, partition_count=4096) as db:
                recovered_size = db.size
                last_seq = db.last_sequence
                durable_seq = db.durable_sequence

                # With unique PUT keys and no DELETEs, accepted durable records must form
                # exactly the recovered sequence prefix.
                prefix_ok = recovered_size == last_seq == durable_seq
                known_durable_ok = last_seq >= progress_ticket

                next_key = f"AFTER_{ms}".encode()
                db.put_sync(next_key, b"OK")
                continuation_ok = db.get(next_key) == b"OK"
                db.checkpoint()

            with Database(dbdir, wal_batch=512, partition_count=4096) as reopened:
                continuation_ok = continuation_ok and reopened.get(next_key) == b"OK"

            passed = int(child_signal == signal.SIGKILL and prefix_ok and known_durable_ok and continuation_ok)
            failures += 1 - passed
            print(f"{ms},{child_signal},{progress_ticket},{recovered_size},{last_seq},{durable_seq},{int(continuation_ok)},{passed}")

    return 2 if failures else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--child", nargs=2, metavar=("DBDIR", "PROGRESS"))
    ap.add_argument("--kill-ms", default="5,10,25,50")
    args = ap.parse_args()
    if args.child:
        return child_mode(args.child[0], args.child[1])
    return parent_mode([int(x) for x in args.kill_ms.split(",") if x])


if __name__ == "__main__":
    raise SystemExit(main())
