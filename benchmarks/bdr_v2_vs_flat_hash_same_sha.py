"""Controlled benchmark: BDR V2 vs flat hash table using the same SHA-256 cost.

Purpose
-------
Separate encoder cost from storage-layout cost. Both competitors compute one
SHA-256 per operation. BDR splits the digest into rho/phi/signature; the flat
control uses the first 192 bits directly as a dictionary key.

This benchmark reports median lookup latency for hits and misses, insertion
time, fidelity, and BDR bucket occupancy.
"""

from __future__ import annotations

import argparse
import csv
import gc
import hashlib
from pathlib import Path
import random
import statistics
import time


class BDRV2:
    def __init__(self, space_size_rho: int = 100_000, phase_buckets: int = 65_536):
        self.space_size_rho = space_size_rho
        self.phase_buckets = phase_buckets
        self.buckets = [None] * space_size_rho

    def _encode(self, key: str):
        h = hashlib.sha256(key.encode("utf-8")).digest()
        rho = int.from_bytes(h[0:8], "big") % self.space_size_rho
        phi = int.from_bytes(h[8:16], "big") % self.phase_buckets
        sig = int.from_bytes(h[16:24], "big")
        return rho, phi, sig

    def insert(self, key: str, value: int) -> None:
        rho, phi, sig = self._encode(key)
        bucket = self.buckets[rho]
        if bucket is None:
            bucket = {}
            self.buckets[rho] = bucket
        bucket[(phi, sig)] = (key, value)

    def query(self, key: str):
        rho, phi, sig = self._encode(key)
        bucket = self.buckets[rho]
        if bucket is None:
            return None
        item = bucket.get((phi, sig))
        return item[1] if item and item[0] == key else None

    def occupancy(self):
        occupied = [len(bucket) for bucket in self.buckets if bucket]
        return {
            "occupied_buckets": len(occupied),
            "avg_occupied_bucket": (sum(occupied) / len(occupied)) if occupied else 0.0,
            "max_bucket": max(occupied) if occupied else 0,
        }


class FlatHashSameSHA:
    """Flat control with the same SHA-256 work per operation.

    It uses 192 digest bits as the dictionary key so that the amount of digest
    material consumed is comparable to BDR V2's rho/phi/signature slices.
    """

    def __init__(self):
        self.data = {}

    @staticmethod
    def _fingerprint(key: str) -> bytes:
        return hashlib.sha256(key.encode("utf-8")).digest()[:24]

    def insert(self, key: str, value: int) -> None:
        self.data[self._fingerprint(key)] = (key, value)

    def query(self, key: str):
        item = self.data.get(self._fingerprint(key))
        return item[1] if item and item[0] == key else None


def median_us(fn, queries, repeats=5):
    samples = []
    for _ in range(repeats):
        t0 = time.perf_counter_ns()
        for q in queries:
            fn(q)
        elapsed = time.perf_counter_ns() - t0
        samples.append(elapsed / len(queries) / 1000.0)
    return statistics.median(samples)


def run_one(n: int, m: int, query_count: int, seed: int):
    rng = random.Random(seed + n)
    keys = [f"ETBRA_KEY_{i:09d}" for i in range(n)]
    hits = [keys[rng.randrange(n)] for _ in range(query_count)]
    misses = [f"ETBRA_MISS_{rng.randrange(10**15):015d}" for _ in range(query_count)]

    bdr = BDRV2(space_size_rho=m)
    flat = FlatHashSameSHA()

    t0 = time.perf_counter()
    for i, key in enumerate(keys):
        bdr.insert(key, i)
    bdr_insert_s = time.perf_counter() - t0

    t0 = time.perf_counter()
    for i, key in enumerate(keys):
        flat.insert(key, i)
    flat_insert_s = time.perf_counter() - t0

    for q in hits[:100]:
        bdr.query(q)
        flat.query(q)

    gc.disable()
    try:
        bdr_hit_us = median_us(bdr.query, hits)
        flat_hit_us = median_us(flat.query, hits)
        bdr_miss_us = median_us(bdr.query, misses)
        flat_miss_us = median_us(flat.query, misses)
    finally:
        gc.enable()

    fidelity = sum(bdr.query(q) is not None for q in hits) / len(hits)
    occ = bdr.occupancy()

    return {
        "N": n,
        "M": m,
        "lambda": n / m,
        "bdr_hit_us": bdr_hit_us,
        "flat_hit_us": flat_hit_us,
        "bdr_miss_us": bdr_miss_us,
        "flat_miss_us": flat_miss_us,
        "bdr_insert_s": bdr_insert_s,
        "flat_insert_s": flat_insert_s,
        "fidelity": fidelity,
        **occ,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sizes", default="10000,50000,100000,500000,1000000,2000000")
    parser.add_argument("--M", type=int, default=100_000)
    parser.add_argument("--queries", type=int, default=5_000)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", default="benchmarks/results/bdr_v2_vs_flat_hash_same_sha.csv")
    args = parser.parse_args()

    sizes = [int(x) for x in args.sizes.split(",") if x.strip()]
    rows = [run_one(n, args.M, args.queries, args.seed) for n in sizes]

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    for row in rows:
        print(row)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
