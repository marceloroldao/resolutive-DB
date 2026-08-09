"""BDR V3 stress benchmark: collision width and density scaling.

Purpose
-------
This benchmark separates two independent stressors:
1. rho density lambda=N/M while keeping phase/signature width large;
2. address-width reduction (phase buckets + signature bits) at fixed N/M.

The script reports lookup latency, fidelity, overwrite collisions, mean occupied
rho-bucket size and maximum rho-bucket size. It is intentionally adversarial:
it is designed to discover failure thresholds rather than confirm O(1).
"""

from __future__ import annotations

import gc
import hashlib
import random
import statistics
import time


class BDRV3Stress:
    def __init__(self, M=100_000, phase_buckets=65_536, signature_bits=64):
        self.M = M
        self.phase_buckets = phase_buckets
        self.signature_bits = signature_bits
        self.signature_mask = (1 << signature_bits) - 1
        self.buckets = [None] * M
        self.count = 0
        self.overwrites = 0

    def encode(self, key: str):
        h = hashlib.sha256(key.encode("utf-8")).digest()
        rho = int.from_bytes(h[0:8], "big") % self.M
        phi = int.from_bytes(h[8:16], "big") % self.phase_buckets
        sig = int.from_bytes(h[16:24], "big") & self.signature_mask
        return rho, phi, sig

    def insert(self, key: str, payload):
        rho, phi, sig = self.encode(key)
        if self.buckets[rho] is None:
            self.buckets[rho] = {}
        slot = (phi, sig)
        old = self.buckets[rho].get(slot)
        if old is None:
            self.count += 1
        elif old[0] != key:
            self.overwrites += 1
        self.buckets[rho][slot] = (key, payload)

    def query(self, key: str):
        rho, phi, sig = self.encode(key)
        bucket = self.buckets[rho]
        if bucket is None:
            return None
        item = bucket.get((phi, sig))
        if item and item[0] == key:
            return item[1]
        return None


def median_lookup_us(db, queries, repeats=5):
    values = []
    gc.disable()
    try:
        for _ in range(repeats):
            t0 = time.perf_counter_ns()
            for q in queries:
                db.query(q)
            elapsed = time.perf_counter_ns() - t0
            values.append(elapsed / len(queries) / 1000.0)
    finally:
        gc.enable()
    return statistics.median(values)


def run_case(N, M, phase_buckets, signature_bits, query_count=5000, seed=42):
    rng = random.Random(seed + N + M + phase_buckets + signature_bits)
    db = BDRV3Stress(M, phase_buckets, signature_bits)
    keys = [f"K_{i:09d}" for i in range(N)]
    for i, key in enumerate(keys):
        db.insert(key, i)
    queries = [keys[rng.randrange(N)] for _ in range(query_count)]
    recovered = sum(db.query(q) is not None for q in queries)
    occupied_sizes = [len(bucket) for bucket in db.buckets if bucket]
    return {
        "N": N,
        "M": M,
        "lambda": N / M,
        "phase_buckets": phase_buckets,
        "signature_bits": signature_bits,
        "lookup_us": median_lookup_us(db, queries),
        "fidelity_pct": recovered * 100.0 / len(queries),
        "overwrites": db.overwrites,
        "stored": db.count,
        "mean_occupied_bucket": sum(occupied_sizes) / len(occupied_sizes),
        "max_bucket": max(occupied_sizes),
    }


def main():
    print("=== ADDRESS-WIDTH STRESS: N=500000, M=100000 ===")
    for phase, sig in [
        (65536, 64),
        (1024, 32),
        (256, 24),
        (64, 16),
        (16, 12),
        (4, 8),
    ]:
        print(run_case(500_000, 100_000, phase, sig))

    print("\n=== DENSITY STRESS: N=500000, full-width address ===")
    for M in [100_000, 20_000, 10_000, 5_000]:
        print(run_case(500_000, M, 65_536, 64, query_count=3000, seed=7))


if __name__ == "__main__":
    main()
