"""Experimental Python bridge for BDR v1.1 AtomicDatabase semantics.

This module is additive: it loads the native atomic C ABI and does not
reimplement WAL, recovery, sequence, or durability semantics in Python.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import sys
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import Iterable, Mapping, Sequence


class AtomicBDRError(RuntimeError):
    """Base error for the native AtomicBDR bridge."""


class AtomicBDRInvalidArgument(AtomicBDRError, ValueError):
    """Raised when the native bridge rejects an argument."""


class AtomicBDRIOError(AtomicBDRError, OSError):
    """Raised when the native bridge reports an I/O failure."""


class DurabilityMode(IntEnum):
    ASYNC = 0
    BATCH_SYNC = 1
    PER_OPERATION_SYNC = 2


class OperationType(IntEnum):
    PUT = 1
    DELETE = 2


@dataclass(frozen=True)
class Operation:
    type: OperationType
    key: str | bytes
    value: str | bytes | bytearray | memoryview | None = None

    @classmethod
    def put(cls, key: str | bytes, value: str | bytes | bytearray | memoryview) -> "Operation":
        return cls(OperationType.PUT, key, value)

    @classmethod
    def delete(cls, key: str | bytes) -> "Operation":
        return cls(OperationType.DELETE, key, None)


@dataclass(frozen=True)
class BatchResult:
    sequence: int
    operations: int
    durable: bool


@dataclass(frozen=True)
class AtomicDiagnostics:
    wal_bytes: int
    replayed_batches: int
    replayed_operations: int
    legacy_records: int
    resident_records: int
    legacy_sequence: int
    last_sequence: int
    durable_sequence: int
    repaired_torn_tail: bool
    legacy_load_us: int
    wal_read_us: int
    wal_decode_apply_us: int
    wal_replay_us: int


class _COperation(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("key", ctypes.c_void_p),
        ("key_size", ctypes.c_size_t),
        ("value", ctypes.c_void_p),
        ("value_size", ctypes.c_size_t),
    ]


class _CBuffer(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_size_t)]


class _CBatchResult(ctypes.Structure):
    _fields_ = [
        ("sequence", ctypes.c_uint64),
        ("operations", ctypes.c_size_t),
        ("durable", ctypes.c_int),
    ]


class _CDiagnostics(ctypes.Structure):
    _fields_ = [
        ("wal_bytes", ctypes.c_uint64),
        ("replayed_batches", ctypes.c_size_t),
        ("replayed_operations", ctypes.c_size_t),
        ("legacy_records", ctypes.c_size_t),
        ("resident_records", ctypes.c_size_t),
        ("legacy_sequence", ctypes.c_uint64),
        ("last_sequence", ctypes.c_uint64),
        ("durable_sequence", ctypes.c_uint64),
        ("repaired_torn_tail", ctypes.c_int),
        ("legacy_load_us", ctypes.c_uint64),
        ("wal_read_us", ctypes.c_uint64),
        ("wal_decode_apply_us", ctypes.c_uint64),
        ("wal_replay_us", ctypes.c_uint64),
    ]


_STATUS_OK = 0
_STATUS_INVALID_ARGUMENT = 1
_STATUS_NOT_FOUND = 2
_STATUS_IO_ERROR = 3
_STATUS_INTERNAL_ERROR = 4


def _to_bytes(value: str | bytes | bytearray | memoryview, *, field: str) -> bytes:
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, bytes):
        return value
    if isinstance(value, (bytearray, memoryview)):
        return bytes(value)
    raise TypeError(f"{field} must be str or bytes-like")


def _library_names() -> tuple[str, ...]:
    if sys.platform == "win32":
        return ("bdr_atomic_c_api.dll", "libbdr_atomic_c_api.dll")
    if sys.platform == "darwin":
        return ("libbdr_atomic_c_api.dylib",)
    return ("libbdr_atomic_c_api.so",)


def _resolve_library(explicit: str | os.PathLike[str] | None) -> str:
    candidates: list[Path] = []
    if explicit is not None:
        candidates.append(Path(explicit))
    env = os.environ.get("BDR_ATOMIC_LIBRARY")
    if env:
        candidates.append(Path(env))

    package_dir = Path(__file__).resolve().parent
    for name in _library_names():
        candidates.extend((package_dir / name, package_dir.parent / name))

    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)

    found = ctypes.util.find_library("bdr_atomic_c_api")
    if found:
        return found

    searched = ", ".join(str(p) for p in candidates) or "system library path"
    raise AtomicBDRError(
        "native BDR atomic library not found; set BDR_ATOMIC_LIBRARY or pass "
        f"library_path. Searched: {searched}"
    )


def _configure_library(lib: ctypes.CDLL) -> None:
    lib.bdr_atomic_c_abi_version.argtypes = []
    lib.bdr_atomic_c_abi_version.restype = ctypes.c_uint32
    lib.bdr_atomic_c_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
    lib.bdr_atomic_c_open.restype = ctypes.c_int
    lib.bdr_atomic_c_write_batch.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(_COperation), ctypes.c_size_t, ctypes.POINTER(_CBatchResult)
    ]
    lib.bdr_atomic_c_write_batch.restype = ctypes.c_int
    if hasattr(lib, "bdr_atomic_c_write_batch_with_durability"):
        lib.bdr_atomic_c_write_batch_with_durability.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(_COperation),
            ctypes.c_size_t,
            ctypes.c_int,
            ctypes.POINTER(_CBatchResult),
        ]
        lib.bdr_atomic_c_write_batch_with_durability.restype = ctypes.c_int
    lib.bdr_atomic_c_get.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(_CBuffer)
    ]
    lib.bdr_atomic_c_get.restype = ctypes.c_int
    lib.bdr_atomic_c_sync.argtypes = [ctypes.c_void_p]
    lib.bdr_atomic_c_sync.restype = ctypes.c_int
    lib.bdr_atomic_c_last_sequence.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint64)]
    lib.bdr_atomic_c_last_sequence.restype = ctypes.c_int
    lib.bdr_atomic_c_durable_sequence.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint64)]
    lib.bdr_atomic_c_durable_sequence.restype = ctypes.c_int
    if hasattr(lib, "bdr_atomic_c_diagnostics_get"):
        lib.bdr_atomic_c_diagnostics_get.argtypes = [ctypes.c_void_p, ctypes.POINTER(_CDiagnostics)]
        lib.bdr_atomic_c_diagnostics_get.restype = ctypes.c_int
    lib.bdr_atomic_c_free_buffer.argtypes = [_CBuffer]
    lib.bdr_atomic_c_free_buffer.restype = None
    lib.bdr_atomic_c_close.argtypes = [ctypes.c_void_p]
    lib.bdr_atomic_c_close.restype = None


def _raise_status(status: int, action: str) -> None:
    if status == _STATUS_OK:
        return
    if status == _STATUS_INVALID_ARGUMENT:
        raise AtomicBDRInvalidArgument(f"{action}: invalid argument")
    if status == _STATUS_IO_ERROR:
        raise AtomicBDRIOError(f"{action}: native I/O error")
    if status == _STATUS_INTERNAL_ERROR:
        raise AtomicBDRError(f"{action}: native internal error")
    raise AtomicBDRError(f"{action}: native status {status}")


class AtomicBDR:
    """Python-accessible wrapper over the BDR v1.1 atomic native C ABI."""

    def __init__(self, lib: ctypes.CDLL, handle: ctypes.c_void_p, library_path: str):
        self._lib = lib
        self._handle = handle
        self._library_path = library_path
        self._closed = False

    @classmethod
    def open(
        cls,
        path: str | os.PathLike[str],
        *,
        library_path: str | os.PathLike[str] | None = None,
    ) -> "AtomicBDR":
        native = _resolve_library(library_path)
        lib = ctypes.CDLL(native)
        _configure_library(lib)
        abi_version = int(lib.bdr_atomic_c_abi_version())
        if abi_version < 1:
            raise AtomicBDRError(f"unsupported atomic C ABI version: {abi_version}")
        directory = os.fsencode(os.fspath(path))
        if not directory:
            raise AtomicBDRInvalidArgument("path must not be empty")
        handle = ctypes.c_void_p()
        status = lib.bdr_atomic_c_open(directory, ctypes.byref(handle))
        _raise_status(status, "open")
        if not handle.value:
            raise AtomicBDRError("open succeeded without a native handle")
        return cls(lib, handle, native)

    @property
    def library_path(self) -> str:
        return self._library_path

    def _require_open(self) -> None:
        if self._closed or not self._handle.value:
            raise AtomicBDRError("AtomicBDR is closed")

    def get(self, key: str | bytes) -> bytes | None:
        self._require_open()
        key_bytes = _to_bytes(key, field="key")
        if not key_bytes:
            raise AtomicBDRInvalidArgument("key must not be empty")
        key_buffer = ctypes.create_string_buffer(key_bytes, len(key_bytes))
        out = _CBuffer()
        status = self._lib.bdr_atomic_c_get(
            self._handle,
            ctypes.cast(key_buffer, ctypes.c_void_p),
            len(key_bytes),
            ctypes.byref(out),
        )
        if status == _STATUS_NOT_FOUND:
            return None
        _raise_status(status, "get")
        try:
            if out.size == 0:
                return b""
            return ctypes.string_at(out.data, out.size)
        finally:
            self._lib.bdr_atomic_c_free_buffer(out)

    def write_batch(
        self,
        operations: Sequence[Operation],
        *,
        durability: DurabilityMode = DurabilityMode.BATCH_SYNC,
    ) -> BatchResult:
        self._require_open()
        if not operations:
            raise AtomicBDRInvalidArgument("write_batch requires at least one operation")
        c_ops = (_COperation * len(operations))()
        keepalive = []
        for index, op in enumerate(operations):
            if not isinstance(op, Operation):
                raise TypeError("operations must contain Operation values")
            key = _to_bytes(op.key, field="key")
            if not key:
                raise AtomicBDRInvalidArgument("key must not be empty")
            key_buffer = ctypes.create_string_buffer(key, len(key))
            keepalive.append(key_buffer)
            c_ops[index].type = int(op.type)
            c_ops[index].key = ctypes.cast(key_buffer, ctypes.c_void_p)
            c_ops[index].key_size = len(key)
            if op.type == OperationType.PUT:
                if op.value is None:
                    raise AtomicBDRInvalidArgument("PUT requires a value")
                value = _to_bytes(op.value, field="value")
                if value:
                    value_buffer = ctypes.create_string_buffer(value, len(value))
                    keepalive.append(value_buffer)
                    c_ops[index].value = ctypes.cast(value_buffer, ctypes.c_void_p)
                else:
                    c_ops[index].value = None
                c_ops[index].value_size = len(value)
            elif op.type == OperationType.DELETE:
                c_ops[index].value = None
                c_ops[index].value_size = 0
            else:
                raise AtomicBDRInvalidArgument(f"unknown operation type: {op.type!r}")
        result = _CBatchResult()
        durability = DurabilityMode(durability)
        if hasattr(self._lib, "bdr_atomic_c_write_batch_with_durability"):
            status = self._lib.bdr_atomic_c_write_batch_with_durability(
                self._handle, c_ops, len(operations), int(durability), ctypes.byref(result)
            )
        else:
            if durability != DurabilityMode.BATCH_SYNC:
                raise AtomicBDRError(
                    "native library does not expose selectable durability; only BATCH_SYNC is available"
                )
            status = self._lib.bdr_atomic_c_write_batch(
                self._handle, c_ops, len(operations), ctypes.byref(result)
            )
        _raise_status(status, "write_batch")
        return BatchResult(int(result.sequence), int(result.operations), bool(result.durable))

    def put_many(
        self,
        items: Mapping[str | bytes, str | bytes | bytearray | memoryview]
        | Iterable[tuple[str | bytes, str | bytes | bytearray | memoryview]],
        *,
        durability: DurabilityMode = DurabilityMode.BATCH_SYNC,
    ) -> BatchResult:
        pairs = items.items() if isinstance(items, Mapping) else items
        return self.write_batch(
            [Operation.put(key, value) for key, value in pairs], durability=durability
        )

    def erase_many(
        self,
        keys: Iterable[str | bytes],
        *,
        durability: DurabilityMode = DurabilityMode.BATCH_SYNC,
    ) -> BatchResult:
        return self.write_batch([Operation.delete(key) for key in keys], durability=durability)

    def last_sequence(self) -> int:
        self._require_open()
        value = ctypes.c_uint64()
        _raise_status(
            self._lib.bdr_atomic_c_last_sequence(self._handle, ctypes.byref(value)),
            "last_sequence",
        )
        return int(value.value)

    def durable_sequence(self) -> int:
        self._require_open()
        value = ctypes.c_uint64()
        _raise_status(
            self._lib.bdr_atomic_c_durable_sequence(self._handle, ctypes.byref(value)),
            "durable_sequence",
        )
        return int(value.value)

    def diagnostics(self) -> AtomicDiagnostics:
        self._require_open()
        if not hasattr(self._lib, "bdr_atomic_c_diagnostics_get"):
            raise AtomicBDRError("native library does not expose experimental diagnostics")
        value = _CDiagnostics()
        _raise_status(
            self._lib.bdr_atomic_c_diagnostics_get(self._handle, ctypes.byref(value)),
            "diagnostics",
        )
        return AtomicDiagnostics(
            wal_bytes=int(value.wal_bytes),
            replayed_batches=int(value.replayed_batches),
            replayed_operations=int(value.replayed_operations),
            legacy_records=int(value.legacy_records),
            resident_records=int(value.resident_records),
            legacy_sequence=int(value.legacy_sequence),
            last_sequence=int(value.last_sequence),
            durable_sequence=int(value.durable_sequence),
            repaired_torn_tail=bool(value.repaired_torn_tail),
            legacy_load_us=int(value.legacy_load_us),
            wal_read_us=int(value.wal_read_us),
            wal_decode_apply_us=int(value.wal_decode_apply_us),
            wal_replay_us=int(value.wal_replay_us),
        )

    def sync(self) -> int:
        self._require_open()
        _raise_status(self._lib.bdr_atomic_c_sync(self._handle), "sync")
        return self.durable_sequence()

    def close(self) -> None:
        if not self._closed:
            if self._handle.value:
                self._lib.bdr_atomic_c_close(self._handle)
            self._handle = ctypes.c_void_p()
            self._closed = True

    def __enter__(self) -> "AtomicBDR":
        self._require_open()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass
