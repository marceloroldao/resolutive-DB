"""Integrated persistent BDR prototype.

Features:
- deterministic append-only WAL with sequence numbers and CRC32
- segmented WAL files
- atomic snapshot/checkpoint via temp file + fsync + rename + directory fsync
- PUT / DELETE / GET API
- reopen/recovery from snapshot + WAL

This is an experimental v0.x engine, not yet a production database.
"""
from __future__ import annotations

import os
import struct
import zlib
from pathlib import Path

MAGIC = b"BDR1"
HDR = struct.Struct(">QBIQ")  # seq, op, key_len, value
CRC = struct.Struct(">I")
OP_PUT = 1
OP_DEL = 2


class RecoveryError(RuntimeError):
    pass


class PersistentBDR:
    def __init__(self, root: str | os.PathLike[str], segment_ops: int = 10_000):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        self.segment_ops = segment_ops
        self.state: dict[str, int] = {}
        self.seq = 0
        self.segment_index = 0
        self.segment_count = 0
        self._load()
        self._open_segment()

    def _snapshot_path(self) -> Path:
        return self.root / "snapshot.bin"

    def _segment_path(self, index: int) -> Path:
        return self.root / f"wal-{index:06d}.log"

    def _open_segment(self) -> None:
        segments = sorted(self.root.glob("wal-*.log"))
        if segments:
            self.segment_index = max(int(p.stem.split("-")[1]) for p in segments)
        self._fh = open(self._segment_path(self.segment_index), "ab", buffering=0)

    def close(self) -> None:
        fh = getattr(self, "_fh", None)
        if fh is not None:
            fh.close()
            self._fh = None

    def _encode(self, seq: int, op: int, key: str, value: int = 0) -> bytes:
        key_bytes = key.encode("utf-8")
        payload = HDR.pack(seq, op, len(key_bytes), value) + key_bytes
        return payload + CRC.pack(zlib.crc32(payload) & 0xFFFFFFFF)

    def _iter_records(self, path: Path):
        with open(path, "rb") as fh:
            while True:
                header = fh.read(HDR.size)
                if not header:
                    return
                if len(header) < HDR.size:
                    return  # torn final record
                seq, op, key_len, value = HDR.unpack(header)
                key_bytes = fh.read(key_len)
                crc_bytes = fh.read(CRC.size)
                if len(key_bytes) < key_len or len(crc_bytes) < CRC.size:
                    return  # torn final record
                payload = header + key_bytes
                (expected_crc,) = CRC.unpack(crc_bytes)
                if (zlib.crc32(payload) & 0xFFFFFFFF) != expected_crc:
                    raise RecoveryError(f"CRC failure in {path.name} at seq={seq}")
                yield seq, op, key_bytes.decode("utf-8"), value

    def _load(self) -> None:
        snapshot = self._snapshot_path()
        if snapshot.exists():
            data = snapshot.read_bytes()
            if len(data) >= 16 and data[:4] == MAGIC:
                snap_seq, count = struct.unpack(">QI", data[4:16])
                pos = 16
                restored: dict[str, int] = {}
                valid = True
                for _ in range(count):
                    if pos + 12 > len(data):
                        valid = False
                        break
                    key_len, value = struct.unpack(">IQ", data[pos : pos + 12])
                    pos += 12
                    key_bytes = data[pos : pos + key_len]
                    pos += key_len
                    if len(key_bytes) != key_len:
                        valid = False
                        break
                    restored[key_bytes.decode("utf-8")] = value
                if valid:
                    self.state = restored
                    self.seq = snap_seq

        expected_seq = self.seq + 1
        for path in sorted(self.root.glob("wal-*.log")):
            for seq, op, key, value in self._iter_records(path):
                if seq <= self.seq:
                    continue
                if seq != expected_seq:
                    raise RecoveryError(
                        f"sequence gap: expected {expected_seq}, got {seq} in {path.name}"
                    )
                if op == OP_PUT:
                    self.state[key] = value
                elif op == OP_DEL:
                    self.state.pop(key, None)
                else:
                    raise RecoveryError(f"unknown op={op} at seq={seq}")
                self.seq = seq
                expected_seq += 1

    def _append(self, op: int, key: str, value: int = 0, durable: bool = False) -> None:
        self.seq += 1
        self._fh.write(self._encode(self.seq, op, key, value))
        self.segment_count += 1
        if durable:
            os.fsync(self._fh.fileno())
        if self.segment_count >= self.segment_ops:
            self._fh.close()
            self.segment_index += 1
            self.segment_count = 0
            self._fh = open(self._segment_path(self.segment_index), "ab", buffering=0)

    def put(self, key: str, value: int, durable: bool = False) -> None:
        self._append(OP_PUT, key, int(value), durable)
        self.state[key] = int(value)

    def delete(self, key: str, durable: bool = False) -> None:
        self._append(OP_DEL, key, 0, durable)
        self.state.pop(key, None)

    def get(self, key: str) -> int | None:
        return self.state.get(key)

    def checkpoint(self) -> None:
        tmp = self.root / "snapshot.tmp"
        items = sorted(self.state.items())
        with open(tmp, "wb") as fh:
            fh.write(MAGIC + struct.pack(">QI", self.seq, len(items)))
            for key, value in items:
                key_bytes = key.encode("utf-8")
                fh.write(struct.pack(">IQ", len(key_bytes), value))
                fh.write(key_bytes)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, self._snapshot_path())
        dir_fd = os.open(self.root, os.O_DIRECTORY)
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)
