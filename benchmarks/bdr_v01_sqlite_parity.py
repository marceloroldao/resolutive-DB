"""Durability-parity benchmark: PersistentBDR vs SQLite WAL/FULL.

Runs two transaction boundary modes:
1) durable commit per operation
2) group commit in fixed-size batches

The script also verifies final-state equality after reopening both databases.
"""
from __future__ import annotations

import random
import sqlite3
import tempfile
import time
from pathlib import Path

from bdr.persistent_engine import PersistentBDR

OPS = 5000
GROUP = 64
KEYSPACE = 2000
SEED = 42


def workload():
    rng = random.Random(SEED)
    ops = []
    for _ in range(OPS):
        key = f"k{rng.randrange(KEYSPACE)}"
        if rng.random() < 0.78:
            ops.append(("put", key, rng.randrange(1 << 31)))
        else:
            ops.append(("del", key, None))
    return ops


def apply_ref(ops):
    ref = {}
    for op, key, value in ops:
        if op == "put":
            ref[key] = value
        else:
            ref.pop(key, None)
    return ref


def run_bdr(root: Path, ops, grouped: bool):
    db = PersistentBDR(root, segment_ops=1000, group_commit_ops=GROUP)
    start = time.perf_counter()
    if grouped:
        for i, (op, key, value) in enumerate(ops, 1):
            if op == "put":
                db.put(key, value, durable=False)
            else:
                db.delete(key, durable=False)
            if i % GROUP == 0:
                db.flush(durable=True)
        db.flush(durable=True)
    else:
        for op, key, value in ops:
            if op == "put":
                db.put(key, value, durable=True)
            else:
                db.delete(key, durable=True)
    elapsed = time.perf_counter() - start
    db.close()
    reopened = PersistentBDR(root, segment_ops=1000, group_commit_ops=GROUP)
    state = dict(reopened.state)
    reopened.close()
    return elapsed, state


def run_sqlite(path: Path, ops, grouped: bool):
    con = sqlite3.connect(path)
    con.execute("PRAGMA journal_mode=WAL")
    con.execute("PRAGMA synchronous=FULL")
    con.execute("CREATE TABLE kv (k TEXT PRIMARY KEY, v INTEGER NOT NULL)")
    start = time.perf_counter()
    if grouped:
        for start_i in range(0, len(ops), GROUP):
            con.execute("BEGIN IMMEDIATE")
            for op, key, value in ops[start_i:start_i + GROUP]:
                if op == "put":
                    con.execute(
                        "INSERT INTO kv(k,v) VALUES(?,?) "
                        "ON CONFLICT(k) DO UPDATE SET v=excluded.v",
                        (key, value),
                    )
                else:
                    con.execute("DELETE FROM kv WHERE k=?", (key,))
            con.commit()
    else:
        for op, key, value in ops:
            con.execute("BEGIN IMMEDIATE")
            if op == "put":
                con.execute(
                    "INSERT INTO kv(k,v) VALUES(?,?) "
                    "ON CONFLICT(k) DO UPDATE SET v=excluded.v",
                    (key, value),
                )
            else:
                con.execute("DELETE FROM kv WHERE k=?", (key,))
            con.commit()
    elapsed = time.perf_counter() - start
    con.close()

    con = sqlite3.connect(path)
    state = dict(con.execute("SELECT k,v FROM kv"))
    con.close()
    return elapsed, state


def main():
    ops = workload()
    expected = apply_ref(ops)
    print("mode,engine,seconds,ops_per_sec,records,state_equal")
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        for grouped in (False, True):
            mode = f"group{GROUP}" if grouped else "commit_per_op"
            bdr_t, bdr_state = run_bdr(root / f"bdr-{mode}", ops, grouped)
            sql_t, sql_state = run_sqlite(root / f"sqlite-{mode}.db", ops, grouped)
            assert bdr_state == expected
            assert sql_state == expected
            assert bdr_state == sql_state
            print(f"{mode},BDR,{bdr_t:.6f},{len(ops)/bdr_t:.2f},{len(bdr_state)},1")
            print(f"{mode},SQLite,{sql_t:.6f},{len(ops)/sql_t:.2f},{len(sql_state)},1")


if __name__ == "__main__":
    main()
