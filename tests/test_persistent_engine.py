import random

from bdr.persistent_engine import PersistentBDR


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
    # Prefix remains readable; only an incomplete tail operation may be lost.
    assert len(reopened.state) >= 199
    reopened.close()
