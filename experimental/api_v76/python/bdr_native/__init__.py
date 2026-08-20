from __future__ import annotations

import ctypes as C
import ctypes.util
import os
from pathlib import Path
from typing import Optional

BDR_OK = 0
BDR_INVALID_ARGUMENT = 1
BDR_NOT_FOUND = 2
BDR_BUFFER_TOO_SMALL = 3
BDR_ERROR = 4
BDR_C_ABI_VERSION = 1


class _Options(C.Structure):
    _fields_ = [
        ("abi_version", C.c_uint32),
        ("struct_size", C.c_size_t),
        ("reserve_bytes", C.c_size_t),
        ("wal_batch", C.c_size_t),
        ("partition_count", C.c_size_t),
        ("partition_max_load", C.c_double),
        ("keep_size_preallocation", C.c_int),
    ]


def _load_library() -> C.CDLL:
    here = Path(__file__).resolve().parent
    explicit = os.environ.get("BDR_C_LIBRARY")
    candidates = [
        here / "libbdr_c.so",
        here / "libbdr_c.dylib",
        here / "bdr_c.dll",
    ]
    if explicit:
        candidates.insert(0, Path(explicit))
    system_name = ctypes.util.find_library("bdr_c")
    if system_name:
        candidates.append(Path(system_name))

    errors: list[str] = []
    for candidate in candidates:
        try:
            return C.CDLL(str(candidate))
        except OSError as exc:
            errors.append(f"{candidate}: {exc}")
    raise RuntimeError("Unable to load packaged BDR C library. Tried: " + " | ".join(errors))


_lib = _load_library()
_lib.bdr_abi_version.restype = C.c_uint32
_lib.bdr_options_size.restype = C.c_size_t
_lib.bdr_default_options.restype = _Options
_lib.bdr_open.argtypes = [C.c_char_p, C.POINTER(_Options), C.POINTER(C.c_void_p)]
_lib.bdr_open.restype = C.c_int
_lib.bdr_close.argtypes = [C.c_void_p]
_lib.bdr_close.restype = C.c_int
_lib.bdr_put.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t, C.POINTER(C.c_uint64)]
_lib.bdr_put.restype = C.c_int
_lib.bdr_put_sync.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t]
_lib.bdr_put_sync.restype = C.c_int
_lib.bdr_delete.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_uint64)]
_lib.bdr_delete.restype = C.c_int
_lib.bdr_delete_sync.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t]
_lib.bdr_delete_sync.restype = C.c_int
_lib.bdr_get.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
_lib.bdr_get.restype = C.c_int
_lib.bdr_wait.argtypes = [C.c_void_p, C.c_uint64]
_lib.bdr_wait.restype = C.c_int
_lib.bdr_sync.argtypes = [C.c_void_p]
_lib.bdr_sync.restype = C.c_int
_lib.bdr_checkpoint.argtypes = [C.c_void_p]
_lib.bdr_checkpoint.restype = C.c_int
_lib.bdr_size.argtypes = [C.c_void_p, C.POINTER(C.c_size_t)]
_lib.bdr_size.restype = C.c_int
_lib.bdr_last_sequence.argtypes = [C.c_void_p, C.POINTER(C.c_uint64)]
_lib.bdr_last_sequence.restype = C.c_int
_lib.bdr_durable_sequence.argtypes = [C.c_void_p, C.POINTER(C.c_uint64)]
_lib.bdr_durable_sequence.restype = C.c_int
_lib.bdr_last_error.restype = C.c_char_p

if int(_lib.bdr_abi_version()) != BDR_C_ABI_VERSION:
    raise RuntimeError(
        f"BDR C ABI mismatch: binding={BDR_C_ABI_VERSION} library={int(_lib.bdr_abi_version())}"
    )
if int(_lib.bdr_options_size()) != C.sizeof(_Options):
    raise RuntimeError(
        f"BDR options size mismatch: binding={C.sizeof(_Options)} library={int(_lib.bdr_options_size())}"
    )


class BDRException(RuntimeError):
    pass


def _error_text() -> str:
    raw = _lib.bdr_last_error()
    return raw.decode("utf-8", "replace") if raw else "unknown BDR error"


def _check(status: int, expected: tuple[int, ...] = (BDR_OK,)) -> int:
    if status not in expected:
        raise BDRException(f"BDR status={status}: {_error_text()}")
    return status


def _bytes(value: bytes | bytearray | memoryview | str) -> bytes:
    if isinstance(value, str):
        return value.encode("utf-8")
    return bytes(value)


def _ptr(data: bytes):
    if not data:
        return None, C.c_void_p()
    buf = C.create_string_buffer(data, len(data))
    return buf, C.cast(buf, C.c_void_p)


class Database:
    def __init__(self, path: str | os.PathLike[str], *, reserve_bytes: Optional[int] = None,
                 wal_batch: Optional[int] = None, partition_count: Optional[int] = None,
                 partition_max_load: Optional[float] = None,
                 keep_size_preallocation: Optional[bool] = None):
        opt = _lib.bdr_default_options()
        if opt.abi_version != BDR_C_ABI_VERSION or opt.struct_size != C.sizeof(_Options):
            raise RuntimeError("BDR library returned incompatible default options")
        if reserve_bytes is not None:
            opt.reserve_bytes = reserve_bytes
        if wal_batch is not None:
            opt.wal_batch = wal_batch
        if partition_count is not None:
            opt.partition_count = partition_count
        if partition_max_load is not None:
            opt.partition_max_load = partition_max_load
        if keep_size_preallocation is not None:
            opt.keep_size_preallocation = int(keep_size_preallocation)
        handle = C.c_void_p()
        _check(_lib.bdr_open(os.fsencode(path), C.byref(opt), C.byref(handle)))
        self._handle = handle

    def _require_open(self) -> None:
        if not self._handle:
            raise BDRException("database is closed")

    def put(self, key, value) -> int:
        self._require_open()
        k, v = _bytes(key), _bytes(value)
        kb, kp = _ptr(k)
        vb, vp = _ptr(v)
        ticket = C.c_uint64()
        _check(_lib.bdr_put(self._handle, kp, len(k), vp, len(v), C.byref(ticket)))
        return int(ticket.value)

    def put_sync(self, key, value) -> None:
        self._require_open()
        k, v = _bytes(key), _bytes(value)
        kb, kp = _ptr(k)
        vb, vp = _ptr(v)
        _check(_lib.bdr_put_sync(self._handle, kp, len(k), vp, len(v)))

    def delete(self, key) -> int:
        self._require_open()
        k = _bytes(key)
        kb, kp = _ptr(k)
        ticket = C.c_uint64()
        _check(_lib.bdr_delete(self._handle, kp, len(k), C.byref(ticket)))
        return int(ticket.value)

    def delete_sync(self, key) -> None:
        self._require_open()
        k = _bytes(key)
        kb, kp = _ptr(k)
        _check(_lib.bdr_delete_sync(self._handle, kp, len(k)))

    def get(self, key) -> Optional[bytes]:
        self._require_open()
        k = _bytes(key)
        kb, kp = _ptr(k)
        needed = C.c_size_t()
        status = _lib.bdr_get(self._handle, kp, len(k), None, 0, C.byref(needed))
        if status == BDR_NOT_FOUND:
            return None
        _check(status, (BDR_BUFFER_TOO_SMALL, BDR_OK))
        if needed.value == 0:
            return b""
        out = C.create_string_buffer(needed.value)
        _check(_lib.bdr_get(self._handle, kp, len(k), C.cast(out, C.c_void_p), needed.value, C.byref(needed)))
        return out.raw[:needed.value]

    def wait(self, ticket: int) -> None:
        self._require_open()
        _check(_lib.bdr_wait(self._handle, ticket))

    def sync(self) -> None:
        self._require_open()
        _check(_lib.bdr_sync(self._handle))

    def checkpoint(self) -> None:
        self._require_open()
        _check(_lib.bdr_checkpoint(self._handle))

    @property
    def size(self) -> int:
        self._require_open()
        out = C.c_size_t()
        _check(_lib.bdr_size(self._handle, C.byref(out)))
        return int(out.value)

    @property
    def last_sequence(self) -> int:
        self._require_open()
        out = C.c_uint64()
        _check(_lib.bdr_last_sequence(self._handle, C.byref(out)))
        return int(out.value)

    @property
    def durable_sequence(self) -> int:
        self._require_open()
        out = C.c_uint64()
        _check(_lib.bdr_durable_sequence(self._handle, C.byref(out)))
        return int(out.value)

    def close(self) -> None:
        if self._handle:
            handle, self._handle = self._handle, C.c_void_p()
            _check(_lib.bdr_close(handle))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


__all__ = ["Database", "BDRException", "BDR_C_ABI_VERSION"]
