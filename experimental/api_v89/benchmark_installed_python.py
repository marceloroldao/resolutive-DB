from __future__ import annotations

import argparse
import csv
import os
from pathlib import Path
import shutil
import sqlite3
import statistics
import tempfile
import time

from bdr_native import Database


def pct(xs, p):
    if not xs:
        return 0.0
    ys = sorted(xs)
    i = min(len(ys) - 1, int((len(ys) - 1) * p))
    return ys[i]


def key(i):
    return f"k-{i:09d}".encode()


def val(i):
    return (f"value-{i:09d}-" + "x" * 32).encode()


def run_bdr(root: Path, total: int, batch: int):
    path = root / f"bdr-b{batch}"
    waits = []
    t0 = time.perf_counter()
    with Database(path) as db:
        pending = 0
        last = 0
        for i in range(total):
            last = db.put(key(i), val(i))
            pending += 1
            if pending == batch:
                w0 = time.perf_counter_ns()
                db.wait(last)
                waits.append((time.perf_counter_ns() - w0) / 1000.0)
                pending = 0
        if pending:
            w0 = time.perf_counter_ns()
            db.wait(last)
            waits.append((time.perf_counter_ns() - w0) / 1000.0)
        db.sync()
    put_s = time.perf_counter() - t0

    get_lat = []
    errors = 0
    t0 = time.perf_counter()
    with Database(path) as db:
        for i in range(total):
            g0 = time.perf_counter_ns()
            got = db.get(key(i))
            get_lat.append((time.perf_counter_ns() - g0) / 1000.0)
            if got != val(i):
                errors += 1
        if len(db) != total:
            errors += 1
        if db.last_sequence != db.durable_sequence:
            errors += 1
    get_s = time.perf_counter() - t0
    return {
        "engine": "BDR-wheel",
        "batch": batch,
        "total": total,
        "put_ops_s": total / put_s,
        "get_ops_s": total / get_s,
        "wait_p50_us": statistics.median(waits) if waits else 0.0,
        "wait_p95_us": pct(waits, 0.95),
        "wait_p99_us": pct(waits, 0.99),
        "get_p50_us": statistics.median(get_lat),
        "get_p95_us": pct(get_lat, 0.95),
        "get_p99_us": pct(get_lat, 0.99),
        "errors": errors,
    }


def run_sqlite(root: Path, total: int, batch: int):
    path = root / f"sqlite-b{batch}.db"
    c = sqlite3.connect(path)
    c.execute("PRAGMA journal_mode=WAL")
    c.execute("PRAGMA synchronous=FULL")
    c.execute("CREATE TABLE kv(k BLOB PRIMARY KEY, v BLOB NOT NULL)")
    commits = []
    t0 = time.perf_counter()
    for start in range(0, total, batch):
        rows = [(key(i), val(i)) for i in range(start, min(total, start + batch))]
        c.execute("BEGIN IMMEDIATE")
        c.executemany("INSERT INTO kv(k,v) VALUES(?,?)", rows)
        w0 = time.perf_counter_ns()
        c.commit()
        commits.append((time.perf_counter_ns() - w0) / 1000.0)
    put_s = time.perf_counter() - t0
    c.close()

    c = sqlite3.connect(path)
    get_lat = []
    errors = 0
    t0 = time.perf_counter()
    for i in range(total):
        g0 = time.perf_counter_ns()
        row = c.execute("SELECT v FROM kv WHERE k=?", (key(i),)).fetchone()
        get_lat.append((time.perf_counter_ns() - g0) / 1000.0)
        if row is None or bytes(row[0]) != val(i):
            errors += 1
    n = c.execute("SELECT count(*) FROM kv").fetchone()[0]
    if n != total:
        errors += 1
    get_s = time.perf_counter() - t0
    c.close()
    return {
        "engine": "SQLite-python",
        "batch": batch,
        "total": total,
        "put_ops_s": total / put_s,
        "get_ops_s": total / get_s,
        "wait_p50_us": statistics.median(commits) if commits else 0.0,
        "wait_p95_us": pct(commits, 0.95),
        "wait_p99_us": pct(commits, 0.99),
        "get_p50_us": statistics.median(get_lat),
        "get_p95_us": pct(get_lat, 0.95),
        "get_p99_us": pct(get_lat, 0.99),
        "errors": errors,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="v89-results.csv")
    ap.add_argument("--strict-total", type=int, default=12000)
    ap.add_argument("--batch-total", type=int, default=80000)
    args = ap.parse_args()

    root = Path(tempfile.mkdtemp(prefix="bdr-v89-"))
    rows = []
    try:
        for batch, total in [(1, args.strict_total), (128, args.batch_total)]:
            rows.append(run_bdr(root, total, batch))
            rows.append(run_sqlite(root, total, batch))
    finally:
        shutil.rmtree(root, ignore_errors=True)

    fields = list(rows[0].keys())
    with open(args.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    for row in rows:
        print(",".join(str(row[k]) for k in fields))
    if any(r["errors"] for r in rows):
        raise SystemExit(2)


if __name__ == "__main__":
    main()
