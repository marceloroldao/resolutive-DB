"""Integrated persistent BDR prototype (pre-v0.1).

Features:
- deterministic segmented append-only WAL with sequence numbers and CRC32
- thread-safe PUT / DELETE / GET API
- configurable group commit plus explicit durable writes
- versioned atomic snapshot/checkpoint with checksum
- safe WAL retirement after a committed checkpoint
- reopen/recovery from snapshot + WAL with strict sequence-gap detection

This remains an experimental research engine, not a production database.
"""
from __future__ import annotations

import os
import struct
import threading
import zlib
from pathlib import Path

WAL_HDR = struct.Struct(">QBIQ")  # seq, op, key_len, value
CRC = struct.Struct(">I")
OP_PUT = 1
OP_DEL = 2

SNAP_MAGIC_V1 = b"BDR1"
SNAP_MAGIC_V2 = b"BDR2"
SNAP_VERSION = 1
SNAP_HDR_V2 = struct.Struct(">IQI")  # format_version, snap_seq, count


class RecoveryError(RuntimeError):
    pass


class PersistentBDR:
    def __init__(
        self,
        root: str | os.PathLike[str],
        segment_ops: int = 10_000,
        group_commit_ops: int = 64,
    ):
        if segment_ops <= 0:
            raise ValueError("segment_ops must be > 0")
        if group_commit_ops <= 0:
            raise ValueError("group_commit_ops must be > 0")

        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        self.segment_ops = int(segment_ops)
        self.group_commit_ops = int(group_commit_ops)
        self.state: dict[str, int] = {}
        self.seq = 0
        self.segment_index = 0
        self.segment_count = 0
        self._pending_since_fsync = 0
        self._lock = threading.RLock()
        self._fh = None

        self._load()
        self._open_segment()

    def _snapshot_path(self) -> Path:
        return self.root / "snapshot.bin"

    def _segment_path(self, index: int) -> Path:
        return self.root / f"wal-{index:06d}.log"

    def _fsync_dir(self) -> None:
        flags = getattr(os, "O_DIRECTORY", 0)
        dir_fd = os.open(self.root, flags)
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)

    def _open_segment(self) -> None:
        segments = sorted(self.root.glob("wal-*.log"))
        if segments:
            self.segment_index = max(int(p.stem.split("-")[1]) for p in segments)
        path = self._segment_path(self.segment_index)
        self._fh = open(path, "ab", buffering=0)
        # segment_count is used only for rotation policy. Count records already
        # present in the active segment so reopen preserves the threshold.
        self.segment_count = sum(1 for _ in self._iter_records(path)) if path.stat().st_size else 0

    def close(self) -> None:
        with self._lock:
            if self._fh is not None:
                self.flush(durable=True)
                self._fh.close()
                self._fh = None

    def flush(self, durable: bool = True) -> None:
        """Flush pending WAL bytes; durable=True also issues fsync."""
        with self._lock:
            if self._fh is None:
                return
            self._fh.flush()
            if durable and self._pending_since_fsync:
                os.fsync(self._fh.fileno())
                self._pending_since_fsync = 0

    def _encode(self, seq: int, op: int, key: str, value: int = 0) -> bytes:
        key_bytes = key.encode("utf-8")
        payload = WAL_HDR.pack(seq, op, len(key_bytes), value) + key_bytes
        return payload + CRC.pack(zlib.crc32(payload) & 0xFFFFFFFF)

    def _iter_records(self, path: Path):
        with open(path, "rb") as fh:
            while True:
                header = fh.read(WAL_HDR.size)
                if not header:
                    return
                if len(header) < WAL_HDR.size:
                    return  # torn final record
                seq, op, key_len, value = WAL_HDR.unpack(header)
                key_bytes = fh.read(key_len)
                crc_bytes = fh.read(CRC.size)
                if len(key_bytes) < key_len or len(crc_bytes) < CRC.size:
                    return  # torn final record
                payload = header + key_bytes
                (expected_crc,) = CRC.unpack(crc_bytes)
                if (zlib.crc32(payload) & 0xFFFFFFFF) != expected_crc:
                    raise RecoveryError(f"CRC failure in {path.name} at seq={seq}")
                yield seq, op, key_bytes.decode("utf-8"), value

    def _load_snapshot_v1(self, data: bytes) -> tuple[dict[str, int], int] | None:
        if len(data) < 16 or data[:4] != SNAP_MAGIC_V1:
            return None
        snap_seq, count = struct.unpack(">QI", data[4:16])
        pos = 16
        restored: dict[str, int] = {}
        for _ in range(count):
            if pos + 12 > len(data):
                return None
            key_len, value = struct.unpack(">IQ", data[pos : pos + 12])
            pos += 12
            key_bytes = data[pos : pos + key_len]
            pos += key_len
            if len(key_bytes) != key_len:
                return None
            restored[key_bytes.decode("utf-8")] = value
        return restored, snap_seq

    def _load_snapshot_v2(self, data: bytes) -> tuple[dict[str, int], int] | None:
        min_len = 4 + SNAP_HDR_V2.size + CRC.size
        if len(data) < min_len or data[:4] != SNAP_MAGIC_V2:
            return None
        payload = data[:-CRC.size]
        (expected_crc,) = CRC.unpack(data[-CRC.size:])
        if (zlib.crc32(payload) & 0xFFFFFFFF) != expected_crc:
            raise RecoveryError("snapshot CRC failure")

        version, snap_seq, count = SNAP_HDR_V2.unpack(data[4 : 4 + SNAP_HDR_V2.size])
        if version != SNAP_VERSION:
            raise RecoveryError(f"unsupported snapshot format version={version}")

        pos = 4 + SNAP_HDR_V2.size
        limit = len(data) - CRC.size
        restored: dict[str, int] = {}
        for _ in range(count):
            if pos + 12 > limit:
                raise RecoveryError("truncated snapshot entry header")
            key_len, value = struct.unpack(">IQ", data[pos : pos + 12])
            pos += 12
            if pos + key_len > limit:
                raise RecoveryError("truncated snapshot key")
            key_bytes = data[pos : pos + key_len]
            pos += key_len
            restored[key_bytes.decode("utf-8")] = value
        if pos != limit:
            raise RecoveryError("unexpected trailing snapshot bytes")
        return restored, snap_seq

    def _load(self) -> None:
        snapshot = self._snapshot_path()
        if snapshot.exists():
            data = snapshot.read_bytes()
            loaded = self._load_snapshot_v2(data)
            if loaded is None:
                loaded = self._load_snapshot_v1(data)
            if loaded is None:
                raise RecoveryError("invalid snapshot format")
            self.state, self.seq = loaded

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

    def _rotate_segment(self) -> None:
        assert self._fh is not None
        self.flush(durable=True)
        self._fh.close()
        self.segment_index += 1
        self.segment_count = 0
        self._fh = open(self._segment_path(self.segment_index), "ab", buffering=0)
        self._fsync_dir()

    def _append(self, op: int, key: str, value: int = 0, durable: bool = False) -> int:
        # Caller holds _lock.
        assert self._fh is not None
        self.seq += 1
        self._fh.write(self._encode(self.seq, op, key, value))
        self.segment_count += 1
        self._pending_since_fsync += 1

        if durable or self._pending_since_fsync >= self.group_commit_ops:
            self._fh.flush()
            os.fsync(self._fh.fileno())
            self._pending_since_fsync = 0

        if self.segment_count >= self.segment_ops:
            self._rotate_segment()
        return self.seq

    def put(self, key: str, value: int, durable: bool = False) -> int:
        with self._lock:
            seq = self._append(OP_PUT, key, int(value), durable)
            self.state[key] = int(value)
            return seq

    def delete(self, key: str, durable: bool = False) -> int:
        with self._lock:
            seq = self._append(OP_DEL, key, 0, durable)
            self.state.pop(key, None)
            return seq

    def get(self, key: str) -> int | None:
        with self._lock:
            return self.state.get(key)

    def checkpoint(self) -> None:
        """Atomically persist current state and retire WAL history covered by it."""
        with self._lock:
            assert self._fh is not None
            self.flush(durable=True)
            checkpoint_seq = self.seq
            items = sorted(self.state.items())

            body = bytearray()
            body.extend(SNAP_MAGIC_V2)
            body.extend(SNAP_HDR_V2.pack(SNAP_VERSION, checkpoint_seq, len(items)))
            for key, value in items:
                key_bytes = key.encode("utf-8")
                body.extend(struct.pack(">IQ", len(key_bytes), value))
                body.extend(key_bytes)
            checksum = CRC.pack(zlib.crc32(body) & 0xFFFFFFFF)

            tmp = self.root / "snapshot.tmp"
            with open(tmp, "wb") as fh:
                fh.write(body)
                fh.write(checksum)
                fh.flush()
                os.fsync(fh.fileno())
            os.replace(tmp, self._snapshot_path())
            self._fsync_dir()

            # Only after the snapshot rename+directory fsync is durable may WAL
            # history <= checkpoint_seq be retired. Rotate first so the new
            # active WAL is unambiguously post-checkpoint.
            self._fh.close()
            old_segments = sorted(self.root.glob("wal-*.log"))
            self.segment_index += 1
            self.segment_count = 0
            self._pending_since_fsync = 0
            self._fh = open(self._segment_path(self.segment_index), "ab", buffering=0)
            self._fsync_dir()

            active = self._segment_path(self.segment_index)
            for path in old_segments:
                if path != active:
                    path.unlink(missing_ok=True)
            self._fsync_dir()

    @property
    def format_version(self) -> int:
        return SNAP_VERSION
