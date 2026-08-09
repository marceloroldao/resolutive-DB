"""Reproducible BDR v2 fixed-density-space benchmark.

This file is a corrected and instrumented version of the user-provided V2.
It keeps M fixed, scales N, records latency, bucket occupancy, collision/replacement
counts, and lookup fidelity. Results are intended to test scaling empirically;
they do not establish worst-case O(1).
"""

from __future__ import annotations

import bisect
import csv
import gc
import hashlib
import random
import statistics
import time
from pathlib import Path


class EncoderResolutivoV2:
    @staticmethod
    def encode(key_str: str, space_size_rho: int, phase_buckets: int = 65536):
        h = hashlib.sha256(key_str.encode("utf-8")).digest()
        rho = int.from_bytes(h[0:8], "big") % space_size_rho
        phi = int.from_bytes(h[8:16], "big") % phase_buckets
        sig = int.from_bytes(h[16:24], "big")
        return rho, phi, sig


class BancoDeDadosResolutivoV2:
    def __init__(self, space_size_rho: int = 100_000, phase_buckets: int = 65536):
        self.space_size_rho = space_size_rho
        self.phase_buckets = phase_buckets
        self.buckets = [None] * space_size_rho
        self.count = 0
        self.replacements = 0

    def insert(self, key_id: str, payload):
        rho, phi, sig = EncoderResolutivoV2.encode(key_id, self.space_size_rho, self.phase_buckets)
        if self.buckets[rho] is None:
            self.buckets[rho] = {}
        slot = (phi, sig)
        if slot in self.buckets[rho]:
            self.replacements += 1
        else:
            self.count += 1
        self.buckets[rho][slot] = (key_id, payload)

    def query(self, key_id: str):
        rho, phi, sig = EncoderResolutivoV2.encode(key_id, self.space_size_rho, self.phase_buckets)
        bucket = self.buckets[rho]
        if bucket is None:
            return None
        item = bucket.get((phi, sig))
        return item[1] if item and item[0] == key_id else None

    def occupancy(self):
        sizes = [len(bucket) for bucket in self.buckets if bucket]
        return {
            "occupied_buckets": len(sizes),
            "mean_bucket_size": (sum(sizes) / len(sizes)) if sizes else 0.0,
            "max_bucket_size": max(sizes) if sizes else 0,
        }


def median_us(fn, queries, repeats=5):
    samples = []
    for _ in range(repeats):
        start = time.perf_counter_ns()
        for query in queries:
            fn(query)
        samples.append((time.perf_counter_ns() - start) / len(queries) / 1000.0)
    return statistics.median(samples)


def benchmark(N, M=100_000, query_count=2000, seed=20260809):
    rng = random.Random(seed + N)
    keys = [f"ETBRA_RESOLUTIVE_KEY_{i:08d}" for i in range(N)]
    bdr = BancoDeDadosResolutivoV2(M)
    py_dict = {}
    for i, key in enumerate(keys):
        bdr.insert(key, i)
        py_dict[key] = i

    # Fixed-width numeric suffixes make this generated list lexicographically sorted.
    sorted_keys = keys
    queries = [keys[rng.randrange(N)] for _ in range(query_count)]

    def binary_lookup(key):
        idx = bisect.bisect_left(sorted_keys, key)
        return sorted_keys[idx] if idx < len(sorted_keys) and sorted_keys[idx] == key else None

    for query in queries[:100]:
        bdr.query(query)
        py_dict.get(query)
        binary_lookup(query)

    gc.disable()
    try:
        bdr_us = median_us(bdr.query, queries)
        dict_us = median_us(py_dict.get, queries)
        binary_us = median_us(binary_lookup, queries)
    finally:
        gc.enable()

    fidelity = sum(bdr.query(q) is not None for q in queries) / len(queries)
    stats = bdr.occupancy()
    return {
        "N": N,
        "M": M,
        "lambda": N / M,
        "bdr_median_us": bdr_us,
        "dict_median_us": dict_us,
        "binary_median_us": binary_us,
        "fidelity": fidelity,
        "replacements": bdr.replacements,
        **stats,
    }


def main():
    sizes = [10_000, 50_000, 100_000, 500_000, 1_000_000, 2_000_000]
    rows = [benchmark(N) for N in sizes]
    out = Path("benchmarks/results")
    out.mkdir(parents=True, exist_ok=True)
    path = out / "bdr_v2_density.csv"
    with path.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    for row in rows:
        print(row)
    print(f"wrote {path}")


if __name__ == "__main__":
    main()
