import os
import struct
import time
import zlib

BASE = "bdr_v8_snapshot_test"
SNAP = BASE + ".snap"
WAL = BASE + ".wal"
MAGIC_SNAP = b"BDRS"
MAGIC_WAL = b"BDRW"


def wal_record(op: int, key: str, value: str = "") -> bytes:
    kb = key.encode("utf-8")
    vb = value.encode("utf-8")
    body = struct.pack("<BII", op, len(kb), len(vb)) + kb + vb
    return MAGIC_WAL + struct.pack("<I", len(body)) + body + struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF)


def append_wal(op: int, key: str, value: str = "") -> None:
    with open(WAL, "ab") as fh:
        fh.write(wal_record(op, key, value))


def replay_wal(db: dict[str, str]) -> tuple[int, int]:
    if not os.path.exists(WAL):
        return 0, 0
    data = open(WAL, "rb").read()
    i = good = bad = 0
    while i < len(data):
        if i + 8 > len(data) or data[i : i + 4] != MAGIC_WAL:
            bad += 1
            break
        body_len = struct.unpack_from("<I", data, i + 4)[0]
        end = i + 8 + body_len + 4
        if end > len(data):
            bad += 1
            break
        body = data[i + 8 : i + 8 + body_len]
        expected_crc = struct.unpack_from("<I", data, i + 8 + body_len)[0]
        if (zlib.crc32(body) & 0xFFFFFFFF) != expected_crc:
            bad += 1
            break
        op, klen, vlen = struct.unpack_from("<BII", body, 0)
        pos = 9
        key = body[pos : pos + klen].decode("utf-8")
        pos += klen
        value = body[pos : pos + vlen].decode("utf-8")
        if op == 1:
            db[key] = value
        elif op == 2:
            db.pop(key, None)
        else:
            bad += 1
            break
        good += 1
        i = end
    return good, bad


def save_snapshot(db: dict[str, str]) -> None:
    tmp = SNAP + ".tmp"
    with open(tmp, "wb") as fh:
        fh.write(MAGIC_SNAP)
        fh.write(struct.pack("<Q", len(db)))
        for key, value in sorted(db.items()):
            kb = key.encode("utf-8")
            vb = value.encode("utf-8")
            body = struct.pack("<II", len(kb), len(vb)) + kb + vb
            fh.write(body)
            fh.write(struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF))
        fh.flush()
        os.fsync(fh.fileno())
    os.replace(tmp, SNAP)
    open(WAL, "wb").close()


def load_snapshot() -> dict[str, str]:
    db: dict[str, str] = {}
    if not os.path.exists(SNAP):
        return db
    data = open(SNAP, "rb").read()
    if data[:4] != MAGIC_SNAP:
        raise ValueError("invalid snapshot magic")
    count = struct.unpack_from("<Q", data, 4)[0]
    i = 12
    for _ in range(count):
        klen, vlen = struct.unpack_from("<II", data, i)
        i += 8
        key_bytes = data[i : i + klen]
        i += klen
        value_bytes = data[i : i + vlen]
        i += vlen
        stored_crc = struct.unpack_from("<I", data, i)[0]
        i += 4
        body = struct.pack("<II", klen, vlen) + key_bytes + value_bytes
        if (zlib.crc32(body) & 0xFFFFFFFF) != stored_crc:
            raise ValueError("snapshot checksum mismatch")
        db[key_bytes.decode("utf-8")] = value_bytes.decode("utf-8")
    return db


def main() -> None:
    for path in (SNAP, WAL):
        try:
            os.remove(path)
        except FileNotFoundError:
            pass

    expected = {f"K{i}": f"V{i}" for i in range(200_000)}

    t0 = time.perf_counter()
    save_snapshot(expected)
    snapshot_write_s = time.perf_counter() - t0

    for i in range(10_000):
        append_wal(1, f"K{i}", f"U{i}")
        expected[f"K{i}"] = f"U{i}"
    for i in range(50_000, 55_000):
        append_wal(2, f"K{i}")
        expected.pop(f"K{i}", None)

    t0 = time.perf_counter()
    reopened = load_snapshot()
    snapshot_load_s = time.perf_counter() - t0
    t0 = time.perf_counter()
    good, bad = replay_wal(reopened)
    wal_replay_s = time.perf_counter() - t0

    print(
        "clean",
        f"snapshot_write_s={snapshot_write_s:.6f}",
        f"snapshot_load_s={snapshot_load_s:.6f}",
        f"wal_replay_s={wal_replay_s:.6f}",
        f"wal_good={good}",
        f"wal_bad={bad}",
        f"live={len(reopened)}",
        f"exact_match={reopened == expected}",
    )

    raw = bytearray(open(WAL, "rb").read())
    raw[len(raw) // 2] ^= 0x55
    open(WAL, "wb").write(raw)

    corrupted = load_snapshot()
    good, bad = replay_wal(corrupted)
    print(
        "middle_corruption",
        f"wal_good={good}",
        f"wal_bad={bad}",
        f"live={len(corrupted)}",
        f"prefix_preserved={good > 0}",
    )


if __name__ == "__main__":
    main()
