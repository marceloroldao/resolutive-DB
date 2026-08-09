"""Reproducible BDR V3 local-engine benchmark.

Compares four lookup engines using the same deterministic SHA-256 encoder:
1. BDR V2: direct rho bucket + Python dict[(phi, signature)]
2. BDR V3 Linear: direct rho bucket + local linear-probing open addressing
3. BDR V3 Robin: direct rho bucket + local Robin Hood hashing
4. Flat hash control: SHA-256 fingerprint -> single Python dict

The benchmark records insertion time, hit/miss median, p95 and p99 latency,
and lookup fidelity. Results are empirical; no worst-case O(1) claim is made.
"""

from __future__ import annotations

import argparse
import csv
import gc
import hashlib
import random
import statistics
import time
from pathlib import Path


def sha_parts(key: str):
    h = hashlib.sha256(key.encode("utf-8")).digest()
    rho = int.from_bytes(h[0:8], "big")
    phi = int.from_bytes(h[8:16], "big")
    sig = int.from_bytes(h[16:24], "big")
    return rho, phi, sig, h


class LinearBucket:
    __slots__ = ("keys", "vals", "mask", "size")

    def __init__(self, capacity: int = 8):
        cap = 1
        while cap < capacity:
            cap <<= 1
        self.keys = [None] * cap
        self.vals = [None] * cap
        self.mask = cap - 1
        self.size = 0

    def _hash(self, key) -> int:
        return hash(key) & self.mask

    def _resize(self):
        old_keys, old_vals = self.keys, self.vals
        self.keys = [None] * (len(old_keys) * 2)
        self.vals = [None] * len(self.keys)
        self.mask = len(self.keys) - 1
        self.size = 0
        for key, value in zip(old_keys, old_vals):
            if key is not None:
                self.set(key, value)

    def set(self, key, value) -> bool:
        if (self.size + 1) / len(self.keys) > 0.70:
            self._resize()
        i = self._hash(key)
        while True:
            current = self.keys[i]
            if current is None:
                self.keys[i] = key
                self.vals[i] = value
                self.size += 1
                return False
            if current == key:
                self.vals[i] = value
                return True
            i = (i + 1) & self.mask

    def get(self, key):
        i = self._hash(key)
        while True:
            current = self.keys[i]
            if current is None:
                return None
            if current == key:
                return self.vals[i]
            i = (i + 1) & self.mask


class RobinBucket:
    __slots__ = ("keys", "vals", "dist", "mask", "size")

    def __init__(self, capacity: int = 8):
        cap = 1
        while cap < capacity:
            cap <<= 1
        self.keys = [None] * cap
        self.vals = [None] * cap
        self.dist = [0] * cap
        self.mask = cap - 1
        self.size = 0

    def _hash(self, key) -> int:
        return hash(key) & self.mask

    def _resize(self):
        old_keys, old_vals = self.keys, self.vals
        self.keys = [None] * (len(old_keys) * 2)
        self.vals = [None] * len(self.keys)
        self.dist = [0] * len(self.keys)
        self.mask = len(self.keys) - 1
        self.size = 0
        for key, value in zip(old_keys, old_vals):
            if key is not None:
                self.set(key, value)

    def set(self, key, value) -> bool:
        if (self.size + 1) / len(self.keys) > 0.70:
            self._resize()
        i = self._hash(key)
        d = 0
        while True:
            current = self.keys[i]
            if current is None:
                self.keys[i] = key
                self.vals[i] = value
                self.dist[i] = d
                self.size += 1
                return False
            if current == key:
                self.vals[i] = value
                return True
            if self.dist[i] < d:
                self.keys[i], key = key, self.keys[i]
                self.vals[i], value = value, self.vals[i]
                self.dist[i], d = d, self.dist[i]
            i = (i + 1) & self.mask
            d += 1

    def get(self, key):
        i = self._hash(key)
        d = 0
        while True:
            current = self.keys[i]
            if current is None or d > self.dist[i]:
                return None
            if current == key:
                return self.vals[i]
            i = (i + 1) & self.mask
            d += 1


class BDRV2:
    def __init__(self, m: int):
        self.m = m
        self.buckets = [None] * m

    def insert(self, key: str, value):
        rho, phi, sig, _ = sha_parts(key)
        idx = rho % self.m
        if self.buckets[idx] is None:
            self.buckets[idx] = {}
        self.buckets[idx][(phi % 65536, sig)] = (key, value)

    def query(self, key: str):
        rho, phi, sig, _ = sha_parts(key)
        bucket = self.buckets[rho % self.m]
        if bucket is None:
            return None
        item = bucket.get((phi % 65536, sig))
        return item[1] if item and item[0] == key else None


class BDRV3:
    def __init__(self, m: int, bucket_cls):
        self.m = m
        self.bucket_cls = bucket_cls
        self.buckets = [None] * m

    def insert(self, key: str, value):
        rho, phi, sig, _ = sha_parts(key)
        idx = rho % self.m
        bucket = self.buckets[idx]
        if bucket is None:
            bucket = self.bucket_cls()
            self.buckets[idx] = bucket
        bucket.set((phi, sig), (key, value))

    def query(self, key: str):
        rho, phi, sig, _ = sha_parts(key)
        bucket = self.buckets[rho % self.m]
        if bucket is None:
            return None
        item = bucket.get((phi, sig))
        return item[1] if item is not None and item[0] == key else None


class FlatHash:
    def __init__(self):
        self.data = {}

    def insert(self, key: str, value):
        _, _, _, digest = sha_parts(key)
        self.data[digest[:16]] = (key, value)

    def query(self, key: str):
        _, _, _, digest = sha_parts(key)
        item = self.data.get(digest[:16])
        return item[1] if item and item[0] == key else None


def percentile(sorted_values, p: float):
    return sorted_values[int(p * (len(sorted_values) - 1))]


def latency_stats(fn, queries):
    values = []
    for q in queries:
        t0 = time.perf_counter_ns()
        fn(q)
        values.append(time.perf_counter_ns() - t0)
    values.sort()
    return {
        "median_us": statistics.median(values) / 1000.0,
        "p95_us": percentile(values, 0.95) / 1000.0,
        "p99_us": percentile(values, 0.99) / 1000.0,
    }


def benchmark(n: int, m: int, query_count: int, seed: int):
    keys = [f"K{i:08d}" for i in range(n)]
    rng = random.Random(seed + n)
    hits = [keys[rng.randrange(n)] for _ in range(query_count)]
    misses = [f"MISS_{rng.randrange(10**12):012d}" for _ in range(query_count)]

    engines = [
        ("bdr_v2_dict", BDRV2(m)),
        ("bdr_v3_linear", BDRV3(m, LinearBucket)),
        ("bdr_v3_robin", BDRV3(m, RobinBucket)),
        ("flat_hash", FlatHash()),
    ]

    rows = []
    for name, engine in engines:
        t0 = time.perf_counter()
        for i, key in enumerate(keys):
            engine.insert(key, i)
        insertion_s = time.perf_counter() - t0

        for q in hits[:100]:
            engine.query(q)

        gc.disable()
        try:
            hit_stats = latency_stats(engine.query, hits)
            miss_stats = latency_stats(engine.query, misses)
        finally:
            gc.enable()

        fidelity = sum(engine.query(q) is not None for q in hits) / len(hits)
        rows.append({
            "N": n,
            "M": m,
            "lambda": n / m,
            "engine": name,
            "insert_s": insertion_s,
            "hit_median_us": hit_stats["median_us"],
            "hit_p95_us": hit_stats["p95_us"],
            "hit_p99_us": hit_stats["p99_us"],
            "miss_median_us": miss_stats["median_us"],
            "miss_p95_us": miss_stats["p95_us"],
            "miss_p99_us": miss_stats["p99_us"],
            "fidelity": fidelity,
        })
    return rows


def parse_sizes(text: str):
    return [int(x) for x in text.split(",") if x.strip()]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sizes", default="10000,100000,300000,500000,1000000")
    parser.add_argument("--m", type=int, default=10000)
    parser.add_argument("--queries", type=int, default=2000)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", default="benchmarks/results/bdr_v3_local_engines.csv")
    args = parser.parse_args()

    rows = []
    for n in parse_sizes(args.sizes):
        rows.extend(benchmark(n, args.m, args.queries, args.seed))

    path = Path(args.output)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    for row in rows:
        print(row)
    print(f"wrote {path}")


if __name__ == "__main__":
    main()
