import argparse
import csv
import random
import shutil
import tempfile
import time

from bdr_native import Database


def run(cycles: int, puts_per_cycle: int, seed: int, output: str) -> None:
    rng = random.Random(seed)
    root = tempfile.mkdtemp(prefix="bdr-v66-")
    oracle: dict[bytes, bytes] = {}
    rows = []
    next_id = 0

    try:
        for cycle in range(cycles):
            t0 = time.perf_counter()
            with Database(root, reserve_bytes=16 * 1024 * 1024,
                          wal_batch=128, partition_count=256,
                          partition_max_load=0.78) as db:
                # Add deterministic unique keys.
                last_ticket = 0
                for _ in range(puts_per_cycle):
                    key = f"k-{next_id:012d}".encode()
                    value = f"cycle={cycle};id={next_id};rnd={rng.getrandbits(64):016x}".encode()
                    last_ticket = db.put(key, value)
                    oracle[key] = value
                    next_id += 1
                if last_ticket:
                    db.wait(last_ticket)

                # Update a sample of existing keys.
                keys = list(oracle)
                rng.shuffle(keys)
                for key in keys[: min(100, len(keys))]:
                    value = b"updated:" + str(cycle).encode() + b":" + key
                    db.put_sync(key, value)
                    oracle[key] = value

                # Delete a deterministic sample.
                keys = list(oracle)
                rng.shuffle(keys)
                delete_count = min(max(1, puts_per_cycle // 5), len(keys))
                for key in keys[:delete_count]:
                    db.delete_sync(key)
                    oracle.pop(key, None)

                db.checkpoint()
                if db.size != len(oracle):
                    raise AssertionError(f"cycle={cycle}: size mismatch before close: {db.size} != {len(oracle)}")
                if db.last_sequence != db.durable_sequence:
                    raise AssertionError(f"cycle={cycle}: durability frontier mismatch")
                seq = db.last_sequence

            # Reopen and verify the complete state, not a sample.
            with Database(root, reserve_bytes=16 * 1024 * 1024,
                          wal_batch=128, partition_count=256,
                          partition_max_load=0.78) as db:
                if db.size != len(oracle):
                    raise AssertionError(f"cycle={cycle}: size mismatch after reopen")
                if db.last_sequence != seq or db.durable_sequence != seq:
                    raise AssertionError(f"cycle={cycle}: sequence mismatch after reopen")
                for key, expected in oracle.items():
                    got = db.get(key)
                    if got != expected:
                        raise AssertionError(f"cycle={cycle}: value mismatch for {key!r}")

            elapsed = time.perf_counter() - t0
            rows.append({
                "cycle": cycle,
                "records": len(oracle),
                "sequence": seq,
                "elapsed_s": elapsed,
                "errors": 0,
            })
            print(f"cycle={cycle},records={len(oracle)},seq={seq},elapsed_s={elapsed:.6f},errors=0")

        with open(output, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=["cycle", "records", "sequence", "elapsed_s", "errors"])
            writer.writeheader()
            writer.writerows(rows)
    finally:
        shutil.rmtree(root, ignore_errors=True)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycles", type=int, default=20)
    ap.add_argument("--puts-per-cycle", type=int, default=500)
    ap.add_argument("--seed", type=int, default=660042)
    ap.add_argument("--output", default="v66_python_soak.csv")
    args = ap.parse_args()
    run(args.cycles, args.puts_per_cycle, args.seed, args.output)
