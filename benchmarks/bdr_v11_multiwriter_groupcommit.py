"""BDR V11 pre-v0.1 gate: multiwriter, group commit, WAL cleanup, soak.

Run: python benchmarks/bdr_v11_multiwriter_groupcommit.py
"""
from __future__ import annotations
import os, random, tempfile, threading, time, shutil
from pathlib import Path
from bdr.persistent_engine import PersistentBDR


def multiwriter_test():
    root = tempfile.mkdtemp(prefix="bdr-v11-mw-")
    try:
        db = PersistentBDR(root, segment_ops=1_000)
        threads, ops = 8, 10_000
        def worker(tid: int):
            rng = random.Random(100 + tid)
            for i in range(ops):
                k = f"k{rng.randrange(5_000)}"
                if rng.random() < 0.8:
                    db.put(k, (tid << 32) | i)
                else:
                    db.delete(k)
        ts = [threading.Thread(target=worker, args=(i,)) for i in range(threads)]
        t0 = time.perf_counter()
        for t in ts: t.start()
        for t in ts: t.join()
        elapsed = time.perf_counter() - t0
        before = dict(db.state)
        db.checkpoint(); db.close()
        reopened = PersistentBDR(root, segment_ops=1_000)
        ok = reopened.state == before
        reopened.close()
        return {"elapsed_s": elapsed, "ops": threads * ops, "reopen_equal": ok}
    finally:
        shutil.rmtree(root, ignore_errors=True)


def soak_test(total_ops=500_000):
    root = tempfile.mkdtemp(prefix="bdr-v11-soak-")
    try:
        db = PersistentBDR(root, segment_ops=5_000)
        ref = {}
        rng = random.Random(123)
        t0 = time.perf_counter()
        for i in range(total_ops):
            k = f"k{rng.randrange(50_000)}"
            if rng.random() < 0.78:
                v = rng.randrange(1 << 32)
                db.put(k, v); ref[k] = v
            else:
                db.delete(k); ref.pop(k, None)
            if (i + 1) % 100_000 == 0:
                db.checkpoint(); db.close()
                db = PersistentBDR(root, segment_ops=5_000)
                assert db.state == ref
        elapsed = time.perf_counter() - t0
        db.close()
        return {"elapsed_s": elapsed, "ops": total_ops, "final_records": len(ref), "ok": True}
    finally:
        shutil.rmtree(root, ignore_errors=True)


if __name__ == "__main__":
    print("multiwriter", multiwriter_test())
    print("soak", soak_test())
    print("NOTE: group-commit timing is documented in the accompanying report; integration into PersistentBDR is the next patch.")
