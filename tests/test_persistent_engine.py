import random
import threading

import pytest

from bdr.persistent_engine import PersistentBDR, RecoveryError


def test_reopen_after_put_delete_checkpoint(tmp_path):
    db = PersistentBDR(tmp_path, segment_ops=100)
    expected = {}
    for i in range(1000):
        key = f"k{i}"
        db.put(key, i)
        expected[key] = i
    for i in range(0, 1000, 3):
        key = f"k{i}"
        db.delete(key)
        expected.pop(key, None)
    db.checkpoint()
    db.close()

    reopened = PersistentBDR(tmp_path, segment_ops=100)
    assert reopened.state == expected
    assert reopened.format_version == 1
    reopened.close()


def test_differential_fuzz_reopen(tmp_path):
    for seed in range(5):
        root = tmp_path / f"seed-{seed}"
        db = PersistentBDR(root, segment_ops=300)
        ref = {}
        rng = random.Random(seed)
        for i in range(10_000):
            key = f"k{rng.randrange(2000)}"
            if rng.random() < 0.72:
                value = rng.randrange(1 << 31)
                db.put(key, value)
                ref[key] = value
            else:
                db.delete(key)
                ref.pop(key, None)
            if i in (2500, 6000):
                db.checkpoint()
        db.close()
        reopened = PersistentBDR(root, segment_ops=300)
        assert reopened.state == ref
        reopened.close()


def test_torn_final_record_is_ignored(tmp_path):
    db = PersistentBDR(tmp_path, segment_ops=1000)
    for i in range(200):
        db.put(f"k{i}", i)
    db.close()

    wal = sorted(tmp_path.glob("wal-*.log"))[-1]
    data = wal.read_bytes()
    wal.write_bytes(data[:-7])

    reopened = PersistentBDR(tmp_path, segment_ops=1000)
    assert len(reopened.state) >= 199
    reopened.close()


def test_multiwriter_checkpoint_and_reopen(tmp_path):
    db = PersistentBDR(tmp_path, segment_ops=200, group_commit_ops=32)

    def worker(tid: int):
        rng = random.Random(1000 + tid)
        for i in range(3000):
            key = f"k{rng.randrange(2500)}"
            if rng.random() < 0.82:
                db.put(key, (tid << 32) | i)
            else:
                db.delete(key)

    threads = [threading.Thread(target=worker, args=(tid,)) for tid in range(8)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    before = dict(db.state)
    last_seq = db.seq
    assert last_seq == 8 * 3000

    db.checkpoint()
    db.close()

    reopened = PersistentBDR(tmp_path, segment_ops=200, group_commit_ops=32)
    assert reopened.state == before
    assert reopened.seq == last_seq
    reopened.close()


def test_checkpoint_retires_old_wal_segments(tmp_path):
    db = PersistentBDR(tmp_path, segment_ops=50, group_commit_ops=16)
    for i in range(500):
        db.put(f"k{i}", i)
    assert len(list(tmp_path.glob("wal-*.log"))) > 1

    db.checkpoint()
    segments = sorted(tmp_path.glob("wal-*.log"))
    assert len(segments) == 1
    assert segments[0].stat().st_size == 0

    for i in range(500, 550):
        db.put(f"k{i}", i)
    db.close()

    reopened = PersistentBDR(tmp_path, segment_ops=50, group_commit_ops=16)
    assert len(reopened.state) == 550
    reopened.close()


def test_snapshot_crc_detects_corruption(tmp_path):
    db = PersistentBDR(tmp_path)
    for i in range(100):
        db.put(f"k{i}", i)
    db.checkpoint()
    db.close()

    snapshot = tmp_path / "snapshot.bin"
    data = bytearray(snapshot.read_bytes())
    data[len(data) // 2] ^= 0x5A
    snapshot.write_bytes(data)

    with pytest.raises(RecoveryError, match="snapshot CRC failure"):
        PersistentBDR(tmp_path)


def test_explicit_durable_write_reopens(tmp_path):
    db = PersistentBDR(tmp_path, group_commit_ops=10_000)
    db.put("durable", 123, durable=True)
    db.close()

    reopened = PersistentBDR(tmp_path, group_commit_ops=10_000)
    assert reopened.get("durable") == 123
    reopened.close()
