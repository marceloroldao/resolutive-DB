from __future__ import annotations

import argparse
import csv
import os
import random
import shutil
import sqlite3
import statistics
import tempfile
import time
from pathlib import Path

from bdr_native import Database

SEED = 6302026


def now_ns() -> int:
    return time.perf_counter_ns()


def percentile_us(samples_ns: list[int], p: float) -> float:
    if not samples_ns:
        return 0.0
    xs = sorted(samples_ns)
    idx = min(len(xs) - 1, max(0, int(round((len(xs) - 1) * p))))
    return xs[idx] / 1_000.0


def stats(samples_ns: list[int]) -> tuple[float, float, float, float]:
    if not samples_ns:
        return (0.0, 0.0, 0.0, 0.0)
    return (
        statistics.mean(samples_ns) / 1_000.0,
        statistics.median(samples_ns) / 1_000.0,
        percentile_us(samples_ns, 0.95),
        percentile_us(samples_ns, 0.99),
    )


def make_dataset(n: int) -> tuple[list[bytes], list[bytes]]:
    keys = [f"K_{i:010d}".encode() for i in range(n)]
    values = [f"V_{i:010d}_payload".encode() for i in range(n)]
    return keys, values


def bench_bdr(base: Path, n: int, batch: int, query_count: int) -> dict:
    dbdir = base / f"bdr-n{n}-b{batch}"
    shutil.rmtree(dbdir, ignore_errors=True)
    keys, values = make_dataset(n)

    put_lat = []
    t0 = now_ns()
    with Database(dbdir, wal_batch=512, partition_count=4096, partition_max_load=0.78) as db:
        if batch == 1:
            for k, v in zip(keys, values):
                a = now_ns(); db.put_sync(k, v); put_lat.append(now_ns() - a)
        else:
            pending = []
            for i, (k, v) in enumerate(zip(keys, values), 1):
                a = now_ns(); ticket = db.put(k, v); pending.append(now_ns() - a)
                if i % batch == 0:
                    db.wait(ticket)
            if n % batch:
                db.sync()
            put_lat = pending
        put_elapsed = (now_ns() - t0) / 1e9

        rng = random.Random(SEED + n + batch)
        queries = [keys[rng.randrange(n)] for _ in range(min(query_count, n))]
        get_lat = []
        g0 = now_ns()
        errors = 0
        for q in queries:
            a = now_ns(); val = db.get(q); get_lat.append(now_ns() - a)
            if val is None:
                errors += 1
        get_elapsed = (now_ns() - g0) / 1e9
        size_before = db.size
        seq = db.last_sequence
        durable = db.durable_sequence

    with Database(dbdir, wal_batch=512, partition_count=4096, partition_max_load=0.78) as reopened:
        fidelity_errors = int(reopened.size != n)
        for i in range(0, n, max(1, n // 1000)):
            if reopened.get(keys[i]) != values[i]:
                fidelity_errors += 1
        recovered_size = reopened.size

    pm, pmed, pp95, pp99 = stats(put_lat)
    gm, gmed, gp95, gp99 = stats(get_lat)
    return {
        "engine": "BDR-Python",
        "n": n,
        "batch": batch,
        "put_ops_s": n / put_elapsed,
        "put_mean_us": pm,
        "put_median_us": pmed,
        "put_p95_us": pp95,
        "put_p99_us": pp99,
        "get_ops_s": len(queries) / get_elapsed,
        "get_mean_us": gm,
        "get_median_us": gmed,
        "get_p95_us": gp95,
        "get_p99_us": gp99,
        "size": size_before,
        "recovered_size": recovered_size,
        "last_sequence": seq,
        "durable_sequence": durable,
        "errors": errors + fidelity_errors,
    }


def bench_sqlite(base: Path, n: int, batch: int, query_count: int) -> dict:
    path = base / f"sqlite-n{n}-b{batch}.db"
    if path.exists(): path.unlink()
    keys, values = make_dataset(n)
    con = sqlite3.connect(path)
    con.execute("PRAGMA journal_mode=WAL")
    con.execute("PRAGMA synchronous=FULL")
    con.execute("PRAGMA temp_store=MEMORY")
    con.execute("CREATE TABLE kv(k BLOB PRIMARY KEY, v BLOB NOT NULL) WITHOUT ROWID")

    put_lat = []
    t0 = now_ns()
    if batch == 1:
        for k, v in zip(keys, values):
            a = now_ns()
            con.execute("INSERT INTO kv(k,v) VALUES (?,?)", (k, v))
            con.commit()
            put_lat.append(now_ns() - a)
    else:
        for start in range(0, n, batch):
            chunk = list(zip(keys[start:start+batch], values[start:start+batch]))
            a = now_ns()
            con.execute("BEGIN IMMEDIATE")
            con.executemany("INSERT INTO kv(k,v) VALUES (?,?)", chunk)
            con.commit()
            elapsed = now_ns() - a
            # Amortized latency per operation for batch durability.
            put_lat.extend([elapsed // len(chunk)] * len(chunk))
    put_elapsed = (now_ns() - t0) / 1e9

    rng = random.Random(SEED + n + batch)
    queries = [keys[rng.randrange(n)] for _ in range(min(query_count, n))]
    get_lat = []
    errors = 0
    g0 = now_ns()
    cur = con.cursor()
    for q in queries:
        a = now_ns(); row = cur.execute("SELECT v FROM kv WHERE k=?", (q,)).fetchone(); get_lat.append(now_ns() - a)
        if row is None:
            errors += 1
    get_elapsed = (now_ns() - g0) / 1e9
    size_before = con.execute("SELECT count(*) FROM kv").fetchone()[0]
    con.close()

    con = sqlite3.connect(path)
    con.execute("PRAGMA journal_mode=WAL")
    recovered_size = con.execute("SELECT count(*) FROM kv").fetchone()[0]
    fidelity_errors = int(recovered_size != n)
    for i in range(0, n, max(1, n // 1000)):
        row = con.execute("SELECT v FROM kv WHERE k=?", (keys[i],)).fetchone()
        if row is None or row[0] != values[i]:
            fidelity_errors += 1
    con.close()

    pm, pmed, pp95, pp99 = stats(put_lat)
    gm, gmed, gp95, gp99 = stats(get_lat)
    return {
        "engine": "SQLite-Python",
        "n": n,
        "batch": batch,
        "put_ops_s": n / put_elapsed,
        "put_mean_us": pm,
        "put_median_us": pmed,
        "put_p95_us": pp95,
        "put_p99_us": pp99,
        "get_ops_s": len(queries) / get_elapsed,
        "get_mean_us": gm,
        "get_median_us": gmed,
        "get_p95_us": gp95,
        "get_p99_us": gp99,
        "size": size_before,
        "recovered_size": recovered_size,
        "last_sequence": "",
        "durable_sequence": "",
        "errors": errors + fidelity_errors,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sizes", default="10000,50000")
    ap.add_argument("--batches", default="1,128")
    ap.add_argument("--queries", type=int, default=10000)
    ap.add_argument("--output", default="v63_results.csv")
    args = ap.parse_args()

    sizes = [int(x) for x in args.sizes.split(",") if x]
    batches = [int(x) for x in args.batches.split(",") if x]
    rows = []
    with tempfile.TemporaryDirectory(prefix="bdr-v63-") as td:
        base = Path(td)
        for n in sizes:
            for batch in batches:
                rows.append(bench_bdr(base, n, batch, args.queries))
                rows.append(bench_sqlite(base, n, batch, args.queries))

    fields = list(rows[0].keys())
    with open(args.output, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader(); w.writerows(rows)

    print(",".join(fields))
    for row in rows:
        print(",".join(str(row[k]) for k in fields))
    if any(int(r["errors"]) != 0 for r in rows):
        raise SystemExit(2)


if __name__ == "__main__":
    main()
