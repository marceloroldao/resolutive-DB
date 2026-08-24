import argparse
import csv
import os
import random
import shutil
import tempfile
from pathlib import Path

from bdr_native import Database


def current_rss_kb() -> int:
    try:
        with open('/proc/self/status', 'r', encoding='utf-8') as f:
            for line in f:
                if line.startswith('VmRSS:'):
                    return int(line.split()[1])
    except OSError:
        pass
    return -1


def dir_bytes(root: Path) -> int:
    total = 0
    for p in root.iterdir():
        if p.is_file():
            total += p.stat().st_size
    return total


def run(cycles: int, puts_per_cycle: int, seed: int, output: str) -> None:
    rng = random.Random(seed)
    root = Path(tempfile.mkdtemp(prefix='bdr-v68-'))
    oracle: dict[bytes, bytes] = {}
    rows = []
    next_id = 0

    try:
        for cycle in range(cycles):
            with Database(root, reserve_bytes=32 * 1024 * 1024,
                          wal_batch=128, partition_count=256,
                          partition_max_load=0.78) as db:
                last_ticket = 0
                for _ in range(puts_per_cycle):
                    key = f'k-{next_id:012d}'.encode()
                    value = f'v-{cycle}-{rng.getrandbits(64):016x}'.encode()
                    last_ticket = db.put(key, value)
                    oracle[key] = value
                    next_id += 1
                if last_ticket:
                    db.wait(last_ticket)

                keys = list(oracle)
                rng.shuffle(keys)
                for key in keys[: min(len(keys), max(1, puts_per_cycle // 4))]:
                    db.delete_sync(key)
                    oracle.pop(key, None)

                db.checkpoint()
                if db.size != len(oracle):
                    raise AssertionError(f'cycle={cycle}: cardinality mismatch')
                if db.last_sequence != db.durable_sequence:
                    raise AssertionError(f'cycle={cycle}: sequence frontier mismatch')

            wals = sorted(root.glob('*.bdw3'))
            snapshots = sorted(root.glob('snapshot.bdr3'))
            temps = sorted(root.glob('snapshot.tmp'))
            if len(wals) != 1:
                raise AssertionError(f'cycle={cycle}: expected exactly 1 WAL, found {len(wals)}')
            if len(snapshots) != 1:
                raise AssertionError(f'cycle={cycle}: expected exactly 1 snapshot')
            if temps:
                raise AssertionError(f'cycle={cycle}: leaked snapshot.tmp')

            wal_size = wals[0].stat().st_size
            snapshot_size = snapshots[0].stat().st_size
            total = dir_bytes(root)
            rss = current_rss_kb()

            with Database(root, reserve_bytes=32 * 1024 * 1024,
                          wal_batch=128, partition_count=256,
                          partition_max_load=0.78) as db:
                if db.size != len(oracle):
                    raise AssertionError(f'cycle={cycle}: reopen size mismatch')
                for key, value in oracle.items():
                    if db.get(key) != value:
                        raise AssertionError(f'cycle={cycle}: reopen payload mismatch')

            rows.append({
                'cycle': cycle,
                'records': len(oracle),
                'wal_count': len(wals),
                'wal_bytes': wal_size,
                'snapshot_bytes': snapshot_size,
                'dir_bytes': total,
                'rss_kb': rss,
                'errors': 0,
            })
            print(
                f'cycle={cycle},records={len(oracle)},wal_count=1,'
                f'wal_bytes={wal_size},snapshot_bytes={snapshot_size},'
                f'dir_bytes={total},rss_kb={rss},errors=0'
            )

        with open(output, 'w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=[
                'cycle', 'records', 'wal_count', 'wal_bytes',
                'snapshot_bytes', 'dir_bytes', 'rss_kb', 'errors'
            ])
            writer.writeheader()
            writer.writerows(rows)
    finally:
        shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--cycles', type=int, default=50)
    ap.add_argument('--puts-per-cycle', type=int, default=200)
    ap.add_argument('--seed', type=int, default=680042)
    ap.add_argument('--output', default='v68_disk_hygiene.csv')
    args = ap.parse_args()
    run(args.cycles, args.puts_per_cycle, args.seed, args.output)
