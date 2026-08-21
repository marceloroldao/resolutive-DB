from __future__ import annotations

import ctypes as _ct
from pathlib import Path
from typing import Optional

BDR_C_ABI_VERSION = 1
BDR_OK = 0
BDR_NOT_FOUND = 1


class BdrError(RuntimeError):
    pass


class Options(_ct.Structure):
    _fields_ = [
        ("abi_version", _ct.c_uint32),
        ("struct_size", _ct.c_uint32),
        ("reserve_bytes", _ct.c_uint64),
        ("wal_batch", _ct.c_uint64),
        ("partition_count", _ct.c_uint64),
        ("partition_max_load", _ct.c_double),
        ("keep_size_preallocation", _ct.c_uint8),
        ("reserved", _ct.c_uint8 * 31),
    ]


def _load_library() -> _ct.CDLL:
    here = Path(__file__).resolve().parent
    candidates = [
        here / "libbdr_c.so.1",
        here / "libbdr_c.so",
        here / "libbdr_c.so.1.0.0",
    ]
    for candidate in candidates:
        if candidate.exists():
            return _ct.CDLL(str(candidate))
    raise ImportError("BDR native library is missing from the installed package")


_lib = _load_library()
_handle_p = _ct.c_void_p
_ticket_t = _ct.c_uint64

_lib.bdr_options_init.argtypes = [_ct.POINTER(Options)]
_lib.bdr_options_init.restype = None
_lib.bdr_abi_version.argtypes = []
_lib.bdr_abi_version.restype = _ct.c_uint32
_lib.bdr_last_error.argtypes = []
_lib.bdr_last_error.restype = _ct.c_char_p
_lib.bdr_open.argtypes = [_ct.c_char_p, _ct.POINTER(Options), _ct.POINTER(_handle_p)]
_lib.bdr_open.restype = _ct.c_int
_lib.bdr_close.argtypes = [_handle_p]
_lib.bdr_close.restype = _ct.c_int
_lib.bdr_put.argtypes = [_handle_p, _ct.c_void_p, _ct.c_size_t, _ct.c_void_p, _ct.c_size_t, _ct.POINTER(_ticket_t)]
_lib.bdr_put.restype = _ct.c_int
_lib.bdr_put_sync.argtypes = [_handle_p, _ct.c_void_p, _ct.c_size_t, _ct.c_void_p, _ct.c_size_t]
_lib.bdr_put_sync.restype = _ct.c_int
_lib.bdr_delete.argtypes = [_handle_p, _ct.c_void_p, _ct.c_size_t, _ct.POINTER(_ticket_t)]
_lib.bdr_delete.restype = _ct.c_int
_lib.bdr_delete_sync.argtypes = [_handle_p, _ct.c_void_p, _ct.c_size_t]
_lib.bdr_delete_sync.restype = _ct.c_int
_lib.bdr_get.argtypes = [_handle_p, _ct.c_void_p, _ct.c_size_t, _ct.c_void_p, _ct.POINTER(_ct.c_size_t)]
_lib.bdr_get.restype = _ct.c_int
_lib.bdr_wait.argtypes = [_handle_p, _ticket_t]
_lib.bdr_wait.restype = _ct.c_int
_lib.bdr_sync.argtypes = [_handle_p]
_lib.bdr_sync.restype = _ct.c_int
_lib.bdr_checkpoint.argtypes = [_handle_p]
_lib.bdr_checkpoint.restype = _ct.c_int
_lib.bdr_last_sequence.argtypes = [_handle_p]
_lib.bdr_last_sequence.restype = _ct.c_uint64
_lib.bdr_durable_sequence.argtypes = [_handle_p]
_lib.bdr_durable_sequence.restype = _ct.c_uint64
_lib.bdr_size.argtypes = [_handle_p]
_lib.bdr_size.restype = _ct.c_uint64

if _lib.bdr_abi_version() != BDR_C_ABI_VERSION:
    raise ImportError(f"incompatible BDR C ABI: {_lib.bdr_abi_version()} != {BDR_C_ABI_VERSION}")


def _error_text() -> str:
    raw = _lib.bdr_last_error()
    return raw.decode("utf-8", "replace") if raw else "BDR operation failed"


def _check(status: int) -> None:
    if status != BDR_OK:
        raise BdrError(f"BDR status={status}: {_error_text()}")


def _as_bytes(value: bytes | bytearray | memoryview | str) -> bytes:
    if isinstance(value, str):
        return value.encode("utf-8")
    return bytes(value)


def _ptr(data: bytes):
    if not data:
        return None, None
    buf = _ct.create_string_buffer(data, len(data))
    return _ct.cast(buf, _ct.c_void_p), buf


class Database:
    def __init__(self, directory: str | Path, *, options: Optional[Options] = None):
        self._handle = _handle_p()
        opts = Options()
        _lib.bdr_options_init(_ct.byref(opts))
        if options is not None:
            opts = options
        status = _lib.bdr_open(str(Path(directory)).encode(), _ct.byref(opts), _ct.byref(self._handle))
        _check(status)
        self._closed = False

    @staticmethod
    def default_options() -> Options:
        opts = Options()
        _lib.bdr_options_init(_ct.byref(opts))
        return opts

    def _require_open(self) -> None:
        if self._closed or not self._handle:
            raise BdrError("database closed")

    def put(self, key, value) -> int:
        self._require_open()
        k, v = _as_bytes(key), _as_bytes(value)
        kp, kb = _ptr(k)
        vp, vb = _ptr(v)
        ticket = _ticket_t()
        _check(_lib.bdr_put(self._handle, kp, len(k), vp, len(v), _ct.byref(ticket)))
        return int(ticket.value)

    def put_sync(self, key, value) -> None:
        self._require_open()
        k, v = _as_bytes(key), _as_bytes(value)
        kp, kb = _ptr(k)
        vp, vb = _ptr(v)
        _check(_lib.bdr_put_sync(self._handle, kp, len(k), vp, len(v)))

    def delete(self, key) -> int:
        self._require_open()
        k = _as_bytes(key)
        kp, kb = _ptr(k)
        ticket = _ticket_t()
        _check(_lib.bdr_delete(self._handle, kp, len(k), _ct.byref(ticket)))
        return int(ticket.value)

    def delete_sync(self, key) -> None:
        self._require_open()
        k = _as_bytes(key)
        kp, kb = _ptr(k)
        _check(_lib.bdr_delete_sync(self._handle, kp, len(k)))

    def get(self, key) -> Optional[bytes]:
        self._require_open()
        k = _as_bytes(key)
        kp, kb = _ptr(k)
        n = _ct.c_size_t(0)
        status = _lib.bdr_get(self._handle, kp, len(k), None, _ct.byref(n))
        if status == BDR_NOT_FOUND:
            return None
        _check(status)
        if n.value == 0:
            return b""
        out = _ct.create_string_buffer(n.value)
        out_n = _ct.c_size_t(n.value)
        _check(_lib.bdr_get(self._handle, kp, len(k), _ct.cast(out, _ct.c_void_p), _ct.byref(out_n)))
        return bytes(out.raw[: out_n.value])

    def wait(self, ticket: int) -> None:
        self._require_open()
        _check(_lib.bdr_wait(self._handle, _ticket_t(ticket)))

    def sync(self) -> None:
        self._require_open()
        _check(_lib.bdr_sync(self._handle))

    def checkpoint(self) -> None:
        self._require_open()
        _check(_lib.bdr_checkpoint(self._handle))

    @property
    def last_sequence(self) -> int:
        self._require_open()
        return int(_lib.bdr_last_sequence(self._handle))

    @property
    def durable_sequence(self) -> int:
        self._require_open()
        return int(_lib.bdr_durable_sequence(self._handle))

    def __len__(self) -> int:
        self._require_open()
        return int(_lib.bdr_size(self._handle))

    def close(self) -> None:
        if self._closed:
            return
        status = _lib.bdr_close(self._handle)
        self._closed = True
        self._handle = _handle_p()
        _check(status)

    def __enter__(self) -> "Database":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def __del__(self):
        if getattr(self, "_closed", True):
            return
        try:
            self.close()
        except Exception:
            pass
