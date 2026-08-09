"""Reproducible lookup-scaling benchmark for the BDR PoC.

This benchmark compares end-to-end point lookup latency for:
- BDR PoC
- Python dict (hash table)
- sorted list + binary search
- SQLite INTEGER PRIMARY KEY index in memory
- linear scan reference

The experiment measures observed latency. It does not assume or pre-label BDR as
proven worst-case O(1). Repeated runs on different hardware and workloads are
required before making stronger complexity or performance claims.
"""

from __future__ import annotations

import argparse
import bisect
import csv
import gc
from pathlib import Path
import random
import sqlite3
import statistics
import time
from typing import Callable

import matplotlib.pyplot as plt

from bdr import BancoDeDadosResolutivo


def median_ns(fn: Callable[[int], object], queries: list[int], repeats: int = 5) -> float:
    samples: list[float] = []
    for _ in range(repeats):
        start = time.perf_counter_ns()
        for q in queries:
            fn(q)
        elapsed = time.perf_counter_ns() - start
        samples.append(elapsed / len(queries))
    return statistics.median(samples)


def benchmark_size(n: int, query_count: int, seed: int) -> dict[str, float | int]:
    rng = random.Random(seed + n)
    keys = list(range(n))
    queries = [rng.randrange(n) for _ in range(min(query_count, n))]

    # BDR -----------------------------------------------------------------
    # Keep a fixed bucket space so growth in N increases density rather than
    # silently resizing the addressing space at every benchmark size.
    bdr = BancoDeDadosResolutivo(bucket_count=1 << 16)
    bdr_start = time.perf_counter()
    for key in keys:
        bdr.inserir(key, str(key))
    bdr_insert_s = time.perf_counter() - bdr_start

    # Python dict ----------------------------------------------------------
    dict_start = time.perf_counter()
    mapping = {key: str(key) for key in keys}
    dict_insert_s = time.perf_counter() - dict_start

    # Sorted-list / binary-search reference -------------------------------
    sorted_keys = keys

    def sorted_lookup(key: int) -> int | None:
        idx = bisect.bisect_left(sorted_keys, key)
        if idx < len(sorted_keys) and sorted_keys[idx] == key:
            return sorted_keys[idx]
        return None

    # SQLite indexed point lookup -----------------------------------------
    conn = sqlite3.connect(":memory:")
    conn.execute("PRAGMA journal_mode=OFF")
    conn.execute("PRAGMA synchronous=OFF")
    conn.execute("CREATE TABLE records (k INTEGER PRIMARY KEY, payload TEXT NOT NULL)")
    sqlite_start = time.perf_counter()
    conn.executemany(
        "INSERT INTO records(k, payload) VALUES (?, ?)",
        ((key, str(key)) for key in keys),
    )
    conn.commit()
    sqlite_insert_s = time.perf_counter() - sqlite_start
    cursor = conn.cursor()

    def sqlite_lookup(key: int) -> str | None:
        row = cursor.execute(
            "SELECT payload FROM records WHERE k = ?", (key,)
        ).fetchone()
        return None if row is None else row[0]

    # Linear-scan reference O(N). Cap the number of queries so large-N runs
    # remain practical.
    linear_queries = queries[: min(50, len(queries))]

    def linear_lookup(key: int) -> int | None:
        for item in keys:
            if item == key:
                return item
        return None

    # Warm-up --------------------------------------------------------------
    for q in queries[: min(100, len(queries))]:
        bdr.buscar(q)
        mapping.get(q)
        sorted_lookup(q)
        sqlite_lookup(q)

    gc.disable()
    try:
        bdr_ns = median_ns(bdr.buscar, queries)
        dict_ns = median_ns(mapping.get, queries)
        sorted_ns = median_ns(sorted_lookup, queries)
        sqlite_ns = median_ns(sqlite_lookup, queries)
        linear_ns = median_ns(linear_lookup, linear_queries, repeats=1)
    finally:
        gc.enable()
        conn.close()

    stats = bdr.estatisticas()
    return {
        "N": n,
        "bdr_ns": bdr_ns,
        "dict_ns": dict_ns,
        "sorted_ns": sorted_ns,
        "sqlite_ns": sqlite_ns,
        "linear_ns": linear_ns,
        "bdr_insert_s": bdr_insert_s,
        "dict_insert_s": dict_insert_s,
        "sqlite_insert_s": sqlite_insert_s,
        "bdr_load_factor": float(stats["load_factor"]),
        "bdr_occupied_buckets": int(stats["occupied_buckets"]),
        "bdr_max_exact_collisions": int(stats["max_exact_collisions"]),
    }


def parse_sizes(text: str) -> list[int]:
    return [int(x.strip()) for x in text.split(",") if x.strip()]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--sizes",
        default="1000,10000,100000,300000,1000000",
        help="comma-separated dataset sizes",
    )
    parser.add_argument("--queries", type=int, default=5000)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output-dir", default="benchmarks/results")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    results = [benchmark_size(n, args.queries, args.seed) for n in parse_sizes(args.sizes)]

    csv_path = output_dir / "latency.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)

    xs = [int(row["N"]) for row in results]
    plt.figure(figsize=(9, 5.5))
    plt.plot(xs, [row["bdr_ns"] for row in results], marker="o", label="BDR PoC")
    plt.plot(xs, [row["dict_ns"] for row in results], marker="o", label="Python dict")
    plt.plot(xs, [row["sorted_ns"] for row in results], marker="o", label="Sorted list / binary search")
    plt.plot(xs, [row["sqlite_ns"] for row in results], marker="o", label="SQLite INTEGER PRIMARY KEY")
    plt.plot(xs, [row["linear_ns"] for row in results], marker="o", label="Linear scan")
    plt.xscale("log")
    plt.yscale("log")
    plt.xlabel("Number of records (N)")
    plt.ylabel("Median lookup latency (ns/query)")
    plt.title("BDR point-lookup scaling benchmark")
    plt.grid(True, which="both", alpha=0.25)
    plt.legend()
    plt.tight_layout()
    png_path = output_dir / "lookup_scaling.png"
    plt.savefig(png_path, dpi=160)

    print(f"wrote {csv_path}")
    print(f"wrote {png_path}")
    for row in results:
        print(row)


if __name__ == "__main__":
    main()
